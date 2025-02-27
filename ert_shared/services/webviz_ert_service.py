from ert_shared.services._base_service import BaseService, local_exec_args


class WebvizErt(BaseService):
    service_name = "webviz-ert"

    def __init__(self, experimental_mode: bool = False):
        exec_args = local_exec_args("webviz_ert")
        if experimental_mode:
            exec_args.append("--experimental-mode")
        super().__init__(exec_args)
