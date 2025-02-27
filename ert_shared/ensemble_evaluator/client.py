import asyncio
import ssl
from typing import AnyStr, Optional, Any, Dict, Union
import cloudevents
from websockets.client import WebSocketClientProtocol, connect
from websockets.exceptions import ConnectionClosed, ConnectionClosedOK
from websockets.datastructures import Headers


class Client:  # pylint: disable=too-many-instance-attributes
    def __enter__(self) -> "Client":
        return self

    def __exit__(self, exc_type: Any, exc_value: Any, exc_traceback: Any) -> None:
        if self.websocket is not None:
            self.loop.run_until_complete(self.websocket.close())
        self.loop.close()

    async def __aenter__(self) -> "Client":
        return self

    async def __aexit__(
        self, exc_type: Any, exc_value: Any, exc_traceback: Any
    ) -> None:
        if self.websocket is not None:
            await self.websocket.close()

    def __init__(  # pylint: disable=too-many-arguments
        self,
        url: str,
        token: Optional[str] = None,
        cert: Optional[Union[str, bytes]] = None,
        max_retries=10,
        timeout_multiplier=5,
    ) -> None:
        if url is None:
            raise ValueError("url was None")
        self.url = url
        self.token = token
        self._extra_headers = Headers()
        if token is not None:
            self._extra_headers["token"] = token

        # Mimics the behavior of the ssl argument when connection to
        # websockets. If none is specified it will deduce based on the url,
        # if True it will enforce TLS, and if you want to use self signed
        # certificates you need to pass an ssl_context with the certificate
        # loaded.
        if cert is not None:
            ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            ssl_context.load_verify_locations(cadata=cert)
        else:
            ssl_context = True if url.startswith("wss") else None
        self._ssl_context = ssl_context

        self._max_retries = max_retries
        self._timeout_multiplier = timeout_multiplier
        self.websocket: Optional[WebSocketClientProtocol] = None
        self.loop = asyncio.new_event_loop()

    async def get_websocket(self) -> WebSocketClientProtocol:
        return await connect(
            self.url, ssl=self._ssl_context, extra_headers=self._extra_headers
        )

    async def _send(self, msg: AnyStr) -> None:
        for retry in range(self._max_retries + 1):
            try:
                if self.websocket is None:
                    self.websocket = await self.get_websocket()
                await self.websocket.send(msg)
                return
            except ConnectionClosedOK:
                # Connection was closed no point in trying to send more messages
                raise
            except (ConnectionClosed, ConnectionRefusedError, OSError):
                if retry == self._max_retries:
                    raise
                await asyncio.sleep(0.2 + self._timeout_multiplier * retry)
                self.websocket = None

    def send(self, msg: AnyStr) -> None:
        self.loop.run_until_complete(self._send(msg))

    def send_event(
        self, ev_type: str, ev_source: str, ev_data: Optional[Dict[str, Any]] = None
    ) -> None:
        if ev_data is None:
            ev_data = {}
        event = cloudevents.http.CloudEvent(
            {
                "type": ev_type,
                "source": ev_source,
                "datacontenttype": "application/json",
            },
            ev_data,
        )
        self.send(cloudevents.http.to_json(event).decode())
