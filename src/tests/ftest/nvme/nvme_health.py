#!/usr/bin/python
'''
  (C) Copyright 2020 Intel Corporation.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  GOVERNMENT LICENSE RIGHTS-OPEN SOURCE SOFTWARE
  The Government's rights to use, modify, reproduce, release, perform, display,
  or disclose this software are subject to the terms of the Apache License as
  provided in Contract No. B609815.
  Any reproduction of computer software, computer software documentation, or
  portions thereof marked with this legend must also reproduce the markings.
'''
import os
from apricot import TestWithServers
from nvme_utils import ServerFillUp, get_device_ids
from test_utils_pool import TestPool
from dmg_utils import DmgCommand
from command_utils_base import CommandFailure

class NvmeHealth(ServerFillUp):
    """
    Test Class Description: To validate NVMe health test cases
    :avocado: recursive
    """
    def test_monitor_for_large_pools(self):
        """
        Test Description: Test Health monitor for large number of pools.
        Use Case: This tests smd's following functions: nvme_list_streams,
                  nvme_get_pool, nvme_set_pool_info, nvme_add_pool,
                  nvme_get_device, nvme_set_device_status,
                  nvme_add_stream_bond, nvme_get_stream_bond

        :avocado: tags=all,hw,medium,nvme,ib2,full_regression
        :avocado: tags=nvme_health,nvme_health_large_pools
        """
        # pylint: disable=attribute-defined-outside-init
        # pylint: disable=too-many-branches
        no_of_pools = self.params.get("number_of_pools", '/run/pool/*')
        #Stop the servers to run SPDK too to get the server capacity
        self.stop_servers_noreset()
        storage = self.get_server_capacity()
        self.start_servers()

        single_pool_nvme_size = int((storage * 0.80)/no_of_pools)
        pools = []
        #Create the Large number of pools
        for _pool in range(no_of_pools):
            pool = TestPool(self.context, dmg_command=self.get_dmg_command())
            pool.get_params(self)
            #SCM size is 10% of NVMe
            pool.scm_size.update('{}GB'.format(single_pool_nvme_size * 0.10))
            pool.nvme_size.update('{}GB'.format(single_pool_nvme_size))
            pool.create()
            pools.append(pool)

        #initialize the dmg command
        self.dmg = DmgCommand(os.path.join(self.prefix, "bin"))
        self.dmg.get_params(self)
        self.dmg.insecure.update(
            self.server_managers[0].get_config_value("allow_insecure"),
            "dmg.insecure")

        #Get Pool query for SMD
        self.dmg.set_sub_command("storage")
        self.dmg.sub_command_class.set_sub_command("query")
        self.dmg.sub_command_class.sub_command_class.set_sub_command("smd")
        for host in self.hostlist_servers:
            self.dmg.sub_command_class. \
                sub_command_class.sub_command_class.devices.value = False
            self.dmg.sub_command_class. \
                sub_command_class.sub_command_class.pools.value = True
            self.dmg.hostlist = host
            try:
                result = self.dmg.run()
            except CommandFailure as error:
                self.fail("dmg command failed: {}".format(error))
            #Verify all pools UUID listed as part of query
            for pool in pools:
                if pool.uuid.lower() not in result.stdout:
                    self.fail('Pool uuid {} not found in smd query'
                              .format(pool.uuid.lower()))

        # Get the device ID
        device_ids = get_device_ids(self.dmg, self.hostlist_servers)

        #Get the device states
        for host in device_ids:
            self.dmg.hostlist = host
            for _dev in device_ids[host]:
                try:
                    result = self.dmg.storage_query_device_state(_dev)
                except CommandFailure as details:
                    self.fail("dmg get device states failed {}".format(details))
                if 'State: NORMAL' not in result.stdout:
                    self.fail("device {} on host {} is not NORMAL"
                              .format(_dev, host))

        #Get the blobstore health data
        for host in device_ids:
            self.dmg.hostlist = host
            for _dev in device_ids[host]:
                try:
                    self.dmg.storage_query_blobstore(_dev)
                except CommandFailure as error:
                    self.fail("dmg get blobstore health failed {}"
                              .format(error))
        #Check nvme-health command works
        try:
            self.dmg.storage_query_nvme_health()
        except CommandFailure as error:
            self.fail("dmg nvme-health failed {}".format(details))

        print('Pool Destroy')
        for pool in pools:
            pool.destroy()
