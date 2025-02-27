# Copyright (C) 2014 Equinor ASA, Norway.
#
#  The file 'ensemble_plot_gen_kw.py' is part of ERT - Ensemble based Reservoir Tool.
#
#  ERT is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  ERT is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.
#
#  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
#  for more details.

from typing import List, Optional

from cwrap import BaseCClass
from ecl.util.util import BoolVector
from res import ResPrototype
from res.enkf.config import EnkfConfigNode
from res.enkf.enkf_fs import EnkfFs
from res.enkf.enums.ert_impl_type_enum import ErtImplType
from res.enkf.plot_data.ensemble_plot_gen_kw_vector import EnsemblePlotGenKWVector


class EnsemblePlotGenKW(BaseCClass):
    TYPE_NAME = "ensemble_plot_gen_kw"

    _alloc = ResPrototype("void* enkf_plot_gen_kw_alloc(enkf_config_node)", bind=False)
    _size = ResPrototype("int   enkf_plot_gen_kw_get_size(ensemble_plot_gen_kw)")
    _load = ResPrototype(
        "void  enkf_plot_gen_kw_load(ensemble_plot_gen_kw, enkf_fs, bool, int, bool_vector)"  # noqa
    )
    _get = ResPrototype(
        "ensemble_plot_gen_kw_vector_ref enkf_plot_gen_kw_iget(ensemble_plot_gen_kw, int)"  # noqa
    )
    _iget_key = ResPrototype(
        "char* enkf_plot_gen_kw_iget_key(ensemble_plot_gen_kw, int)"
    )
    _get_keyword_count = ResPrototype(
        "int   enkf_plot_gen_kw_get_keyword_count(ensemble_plot_gen_kw)"
    )
    _should_use_log_scale = ResPrototype(
        "bool  enkf_plot_gen_kw_should_use_log_scale(ensemble_plot_gen_kw, int)"
    )
    _free = ResPrototype("void  enkf_plot_gen_kw_free(ensemble_plot_gen_kw)")

    def __init__(
        self, ensemble_config_node: EnkfConfigNode, file_system, input_mask=None
    ):
        assert isinstance(ensemble_config_node, EnkfConfigNode)
        assert ensemble_config_node.getImplementationType() == ErtImplType.GEN_KW

        c_pointer = self._alloc(ensemble_config_node)
        super().__init__(c_pointer)

        self.__load(file_system, input_mask)

    def __load(self, file_system: EnkfFs, input_mask: Optional[List[bool]] = None):
        assert isinstance(file_system, EnkfFs)
        if input_mask is None:
            mask = None
        else:
            mask_indices = [idx for idx, value in enumerate(input_mask) if value]
            mask = BoolVector.createFromList(mask_indices)
        self._load(file_system, True, 0, mask)

    def __len__(self):
        """@rtype: int"""
        return self._size()

    def __getitem__(self, index) -> EnsemblePlotGenKWVector:
        """@rtype: EnsemblePlotGenKWVector"""
        return self._get(index)

    def __iter__(self):
        cur = 0
        while cur < len(self):
            yield self[cur]
            cur += 1

    def getKeyWordCount(self):
        """@rtype: int"""
        return self._get_keyword_count()

    def getKeyWordForIndex(self, index):
        """@rtype: str"""
        return self._iget_key(index)

    def getIndexForKeyword(self, keyword):
        """@rtype: int"""
        for index in range(self.getKeyWordCount()):
            kw = self.getKeyWordForIndex(index)
            if kw == keyword:
                return index
        return None

    def shouldUseLogScale(self, index):
        """@rtype: bool"""
        return bool(self._should_use_log_scale(index))

    def free(self):
        self._free()

    def __repr__(self):
        return f"EnsemblePlotGenKW(size = {len(self)}) {self._ad_str()}"
