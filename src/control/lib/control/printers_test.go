//
// (C) Copyright 2020 Intel Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// GOVERNMENT LICENSE RIGHTS-OPEN SOURCE SOFTWARE
// The Government's rights to use, modify, reproduce, release, perform, display,
// or disclose this software are subject to the terms of the Apache License as
// provided in Contract No. 8F-30005.
// Any reproduction of computer software, computer software documentation, or
// portions thereof marked with this legend must also reproduce the markings.
//

package control

import (
	"context"
	"strings"
	"testing"

	"github.com/google/go-cmp/cmp"
	"github.com/pkg/errors"

	"github.com/daos-stack/daos/src/control/common"
	"github.com/daos-stack/daos/src/control/logging"
)

func TestControl_PrintHostErrorsMap(t *testing.T) {
	makeHosts := func(hosts ...string) []string {
		return hosts
	}
	makeErrors := func(errStrings ...string) (errs []error) {
		for _, errStr := range errStrings {
			errs = append(errs, errors.New(errStr))
		}
		return
	}

	for name, tc := range map[string]struct {
		hosts       []string
		errors      []error
		expPrintStr string
	}{
		"one host one error": {
			hosts:  makeHosts("host1"),
			errors: makeErrors("whoops"),
			expPrintStr: `
Hosts Error  
----- -----  
host1 whoops 
`,
		},
		"two hosts one error": {
			hosts:  makeHosts("host1", "host2"),
			errors: makeErrors("whoops", "whoops"),
			expPrintStr: `
Hosts     Error  
-----     -----  
host[1-2] whoops 
`,
		},
		"two hosts one error (sorted)": {
			hosts:  makeHosts("host2", "host1"),
			errors: makeErrors("whoops", "whoops"),
			expPrintStr: `
Hosts     Error  
-----     -----  
host[1-2] whoops 
`,
		},
		"two hosts two errors": {
			hosts:  makeHosts("host1", "host2"),
			errors: makeErrors("whoops", "oops"),
			expPrintStr: `
Hosts Error  
----- -----  
host1 whoops 
host2 oops   
`,
		},
		"two hosts same port one error": {
			hosts:  makeHosts("host1:1", "host2:1"),
			errors: makeErrors("whoops", "whoops"),
			expPrintStr: `
Hosts       Error  
-----       -----  
host[1-2]:1 whoops 
`,
		},
		"two hosts different port one error": {
			hosts:  makeHosts("host1:1", "host2:2"),
			errors: makeErrors("whoops", "whoops"),
			expPrintStr: `
Hosts           Error  
-----           -----  
host1:1,host2:2 whoops 
`,
		},
	} {
		t.Run(name, func(t *testing.T) {
			hem := make(HostErrorsMap)
			for i, host := range tc.hosts {
				if err := hem.Add(host, tc.errors[i]); err != nil {
					t.Fatal(err)
				}
			}

			var bld strings.Builder
			if err := PrintHostErrorsMap(hem, &bld, PrintWithHostPorts()); err != nil {
				t.Fatal(err)
			}

			if diff := cmp.Diff(strings.TrimLeft(tc.expPrintStr, "\n"), bld.String()); diff != "" {
				t.Fatalf("unexpected format string (-want, +got):\n%s\n", diff)
			}
		})
	}
}

func TestControl_PrintStorageScanResponse(t *testing.T) {
	var (
		standardScan      = mockServerScanResponse(t, standard)
		withNamespaceScan = mockServerScanResponse(t, withNamespace)
		noNVMEScan        = mockServerScanResponse(t, noNVME)
		noSCMScan         = mockServerScanResponse(t, noSCM)
		noStorageScan     = mockServerScanResponse(t, noStorage)
		scmScanFailed     = mockServerScanResponse(t, scmFailed)
		nvmeScanFailed    = mockServerScanResponse(t, nvmeFailed)
		bothScansFailed   = mockServerScanResponse(t, bothFailed)
	)
	for name, tc := range map[string]struct {
		mic         *MockInvokerConfig
		expPrintStr string
	}{
		"empty response": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{},
			},
		},
		"server error": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:  "host1",
							Error: errors.New("failed"),
						},
					},
				},
			},
			expPrintStr: `
Errors:
  Hosts Error  
  ----- -----  
  host1 failed 

`,
		},
		"scm scan error": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: scmScanFailed,
						},
					},
				},
			},
			expPrintStr: `
Errors:
  Hosts Error           
  ----- -----           
  host1 scm scan failed 

Hosts SCM Total       NVMe Total         
----- ---------       ----------         
host1 0 B (0 modules) 1 B (1 controller) 
`,
		},
		"nvme scan error": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: nvmeScanFailed,
						},
					},
				},
			},
			expPrintStr: `
Errors:
  Hosts Error            
  ----- -----            
  host1 nvme scan failed 

Hosts SCM Total      NVMe Total          
----- ---------      ----------          
host1 1 B (1 module) 0 B (0 controllers) 
`,
		},
		"scm and nvme scan error": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1:1",
							Message: bothScansFailed,
						},
						{
							Addr:    "host2:1",
							Message: bothScansFailed,
						},
					},
				},
			},
			expPrintStr: `
Errors:
  Hosts     Error            
  -----     -----            
  host[1-2] nvme scan failed 
  host[1-2] scm scan failed  

Hosts     SCM Total       NVMe Total          
-----     ---------       ----------          
host[1-2] 0 B (0 modules) 0 B (0 controllers) 
`,
		},
		"no storage": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: noStorageScan,
						},
					},
				},
			},
			expPrintStr: `
Hosts SCM Total       NVMe Total          
----- ---------       ----------          
host1 0 B (0 modules) 0 B (0 controllers) 
`,
		},
		"single host": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: standardScan,
						},
					},
				},
			},
			expPrintStr: `
Hosts SCM Total      NVMe Total         
----- ---------      ----------         
host1 1 B (1 module) 1 B (1 controller) 
`,
		},
		"single host with namespace": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: withNamespaceScan,
						},
					},
				},
			},
			expPrintStr: `
Hosts SCM Total         NVMe Total         
----- ---------         ----------         
host1 1 B (1 namespace) 1 B (1 controller) 
`,
		},
		"two hosts same scan": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: standardScan,
						},
						{
							Addr:    "host2",
							Message: standardScan,
						},
					},
				},
			},
			expPrintStr: `
Hosts     SCM Total      NVMe Total         
-----     ---------      ----------         
host[1-2] 1 B (1 module) 1 B (1 controller) 
`,
		},
		"two hosts different scans": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: noNVMEScan,
						},
						{
							Addr:    "host2",
							Message: noSCMScan,
						},
					},
				},
			},
			expPrintStr: `
Hosts SCM Total       NVMe Total          
----- ---------       ----------          
host1 1 B (1 module)  0 B (0 controllers) 
host2 0 B (0 modules) 1 B (1 controller)  
`,
		},
		"1024 hosts same scan": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: mockHostResponses(t, 1024, "host%000d", standardScan),
				},
			},
			expPrintStr: `
Hosts        SCM Total      NVMe Total         
-----        ---------      ----------         
host[0-1023] 1 B (1 module) 1 B (1 controller) 
`,
		},
	} {
		t.Run(name, func(t *testing.T) {
			log, buf := logging.NewTestLogger(t.Name())
			defer common.ShowBufferOnFailure(t, buf)

			ctx := context.TODO()
			mi := NewMockInvoker(log, tc.mic)

			resp, err := StorageScan(ctx, mi, &StorageScanReq{})
			if err != nil {
				t.Fatal(err)
			}

			var bld strings.Builder
			if err := PrintResponseErrors(resp, &bld); err != nil {
				t.Fatal(err)
			}
			if err := PrintHostStorageMap(resp.HostStorage, &bld); err != nil {
				t.Fatal(err)
			}

			if diff := cmp.Diff(strings.TrimLeft(tc.expPrintStr, "\n"), bld.String()); diff != "" {
				t.Fatalf("unexpected format string (-want, +got):\n%s\n", diff)
			}
		})
	}
}

func TestControl_PrintStorageScanResponseVerbose(t *testing.T) {
	var (
		standardScan       = mockServerScanResponse(t, standard)
		withNamespacesScan = mockServerScanResponse(t, withNamespace)
		noNVMEScan         = mockServerScanResponse(t, noNVME)
		noSCMScan          = mockServerScanResponse(t, noSCM)
		noStorageScan      = mockServerScanResponse(t, noStorage)
		scmScanFailed      = mockServerScanResponse(t, scmFailed)
		nvmeScanFailed     = mockServerScanResponse(t, nvmeFailed)
		bothScansFailed    = mockServerScanResponse(t, bothFailed)
	)
	for name, tc := range map[string]struct {
		mic         *MockInvokerConfig
		expPrintStr string
	}{
		"empty response": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{},
			},
		},
		"server error": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:  "host1",
							Error: errors.New("failed"),
						},
					},
				},
			},
			expPrintStr: `
Errors:
  Hosts Error  
  ----- -----  
  host1 failed 

`,
		},
		"scm scan error": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: scmScanFailed,
						},
					},
				},
			},
			expPrintStr: `
Errors:
  Hosts Error           
  ----- -----           
  host1 scm scan failed 

-----
host1
-----
	No SCM modules found

NVMe PCI     Model   FW Revision Socket ID Capacity 
--------     -----   ----------- --------- -------- 
0000:80:00.1 model-1 fwRev-1     1         1 B      

`,
		},
		"nvme scan error": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: nvmeScanFailed,
						},
					},
				},
			},
			expPrintStr: `
Errors:
  Hosts Error            
  ----- -----            
  host1 nvme scan failed 

-----
host1
-----
SCM Module ID Socket ID Memory Ctrlr ID Channel ID Channel Slot Capacity 
------------- --------- --------------- ---------- ------------ -------- 
1             1         1               1          1            1 B      

	No NVMe devices found

`,
		},
		"scm and nvme scan error": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1:1",
							Message: bothScansFailed,
						},
						{
							Addr:    "host2:1",
							Message: bothScansFailed,
						},
					},
				},
			},
			expPrintStr: `
Errors:
  Hosts     Error            
  -----     -----            
  host[1-2] nvme scan failed 
  host[1-2] scm scan failed  

---------
host[1-2]
---------
	No SCM modules found

	No NVMe devices found

`,
		},
		"no storage": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: noStorageScan,
						},
						{
							Addr:    "host2",
							Message: noStorageScan,
						},
					},
				},
			},
			expPrintStr: `
---------
host[1-2]
---------
	No SCM modules found

	No NVMe devices found

`,
		},
		"single host": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: standardScan,
						},
					},
				},
			},
			expPrintStr: `
-----
host1
-----
SCM Module ID Socket ID Memory Ctrlr ID Channel ID Channel Slot Capacity 
------------- --------- --------------- ---------- ------------ -------- 
1             1         1               1          1            1 B      

NVMe PCI     Model   FW Revision Socket ID Capacity 
--------     -----   ----------- --------- -------- 
0000:80:00.1 model-1 fwRev-1     1         1 B      

`,
		},
		"single host with namespace": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: withNamespacesScan,
						},
					},
				},
			},
			expPrintStr: `
-----
host1
-----
SCM Namespace Socket ID Capacity 
------------- --------- -------- 
/dev/pmem1    1         1 B      

NVMe PCI     Model   FW Revision Socket ID Capacity 
--------     -----   ----------- --------- -------- 
0000:80:00.1 model-1 fwRev-1     1         1 B      

`,
		},
		"two hosts same scan": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: standardScan,
						},
						{
							Addr:    "host2",
							Message: standardScan,
						},
					},
				},
			},
			expPrintStr: `
---------
host[1-2]
---------
SCM Module ID Socket ID Memory Ctrlr ID Channel ID Channel Slot Capacity 
------------- --------- --------------- ---------- ------------ -------- 
1             1         1               1          1            1 B      

NVMe PCI     Model   FW Revision Socket ID Capacity 
--------     -----   ----------- --------- -------- 
0000:80:00.1 model-1 fwRev-1     1         1 B      

`,
		},
		"two hosts different scans": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: []*HostResponse{
						{
							Addr:    "host1",
							Message: noNVMEScan,
						},
						{
							Addr:    "host2",
							Message: noSCMScan,
						},
					},
				},
			},
			expPrintStr: `
-----
host1
-----
SCM Module ID Socket ID Memory Ctrlr ID Channel ID Channel Slot Capacity 
------------- --------- --------------- ---------- ------------ -------- 
1             1         1               1          1            1 B      

	No NVMe devices found

-----
host2
-----
	No SCM modules found

NVMe PCI     Model   FW Revision Socket ID Capacity 
--------     -----   ----------- --------- -------- 
0000:80:00.1 model-1 fwRev-1     1         1 B      

`,
		},
		"1024 hosts same scan": {
			mic: &MockInvokerConfig{
				UnaryResponse: &UnaryResponse{
					Responses: mockHostResponses(t, 1024, "host%000d", standardScan),
				},
			},
			expPrintStr: `
------------
host[0-1023]
------------
SCM Module ID Socket ID Memory Ctrlr ID Channel ID Channel Slot Capacity 
------------- --------- --------------- ---------- ------------ -------- 
1             1         1               1          1            1 B      

NVMe PCI     Model   FW Revision Socket ID Capacity 
--------     -----   ----------- --------- -------- 
0000:80:00.1 model-1 fwRev-1     1         1 B      

`,
		},
	} {
		t.Run(name, func(t *testing.T) {
			log, buf := logging.NewTestLogger(t.Name())
			defer common.ShowBufferOnFailure(t, buf)

			ctx := context.TODO()
			mi := NewMockInvoker(log, tc.mic)

			resp, err := StorageScan(ctx, mi, &StorageScanReq{})
			if err != nil {
				t.Fatal(err)
			}

			var bld strings.Builder
			if err := PrintResponseErrors(resp, &bld); err != nil {
				t.Fatal(err)
			}
			if err := PrintHostStorageMap(resp.HostStorage, &bld, PrintWithVerboseOutput(true)); err != nil {
				t.Fatal(err)
			}

			if diff := cmp.Diff(strings.TrimLeft(tc.expPrintStr, "\n"), bld.String()); diff != "" {
				t.Fatalf("unexpected format string (-want, +got):\n%s\n", diff)
			}
		})
	}
}

func TestControl_PrintStorageFormatResponse(t *testing.T) {
	for name, tc := range map[string]struct {
		resp        *StorageFormatResp
		expPrintStr string
	}{
		"empty response": {
			resp: &StorageFormatResp{},
		},
		"server error": {
			resp: &StorageFormatResp{
				HostErrorsResp: mockHostErrorsResp(t, &mockHostError{"host1", "failed"}),
			},
			expPrintStr: `
Errors:
  Hosts Error  
  ----- -----  
  host1 failed 

`,
		},
		"2 SCM, 2 NVMe; first SCM fails": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:       1,
				scmPerHost:  2,
				scmFailures: mockFailureMap(0),
				nvmePerHost: 2,
			}),
			expPrintStr: `
Errors:
  Hosts Error                
  ----- -----                
  host1 /mnt/1 format failed 

Format Summary:
  Hosts SCM Devices NVMe Devices 
  ----- ----------- ------------ 
  host1 1           1            
`,
		},
		"2 SCM, 2 NVMe; second NVMe fails": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:        1,
				scmPerHost:   2,
				nvmePerHost:  2,
				nvmeFailures: mockFailureMap(1),
			}),
			expPrintStr: `
Errors:
  Hosts Error                       
  ----- -----                       
  host1 NVMe device 2 format failed 

Format Summary:
  Hosts SCM Devices NVMe Devices 
  ----- ----------- ------------ 
  host1 2           1            
`,
		},
		"2 SCM, 2 NVMe": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:       1,
				scmPerHost:  2,
				nvmePerHost: 2,
			}),
			expPrintStr: `

Format Summary:
  Hosts SCM Devices NVMe Devices 
  ----- ----------- ------------ 
  host1 2           2            
`,
		},
		"2 Hosts, 2 SCM, 2 NVMe; first SCM fails": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:       2,
				scmPerHost:  2,
				scmFailures: mockFailureMap(0),
				nvmePerHost: 2,
			}),
			expPrintStr: `
Errors:
  Hosts     Error                
  -----     -----                
  host[1-2] /mnt/1 format failed 

Format Summary:
  Hosts     SCM Devices NVMe Devices 
  -----     ----------- ------------ 
  host[1-2] 1           1            
`,
		},
		"2 Hosts, 2 SCM, 2 NVMe": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:       2,
				scmPerHost:  2,
				nvmePerHost: 2,
			}),
			expPrintStr: `

Format Summary:
  Hosts     SCM Devices NVMe Devices 
  -----     ----------- ------------ 
  host[1-2] 2           2            
`,
		},
	} {
		t.Run(name, func(t *testing.T) {
			var bld strings.Builder
			if err := PrintResponseErrors(tc.resp, &bld); err != nil {
				t.Fatal(err)
			}
			if err := PrintStorageFormatMap(tc.resp.HostStorage, &bld); err != nil {
				t.Fatal(err)
			}

			if diff := cmp.Diff(strings.TrimLeft(tc.expPrintStr, "\n"), bld.String()); diff != "" {
				t.Fatalf("unexpected format string (-want, +got):\n%s\n", diff)
			}
		})
	}
}

func TestControl_PrintStorageFormatResponseVerbose(t *testing.T) {
	for name, tc := range map[string]struct {
		resp        *StorageFormatResp
		expPrintStr string
	}{
		"empty response": {
			resp: &StorageFormatResp{},
		},
		"server error": {
			resp: &StorageFormatResp{
				HostErrorsResp: mockHostErrorsResp(t, &mockHostError{"host1", "failed"}),
			},
			expPrintStr: `
Errors:
  Hosts Error  
  ----- -----  
  host1 failed 

`,
		},
		"2 SCM, 2 NVMe; first SCM fails": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:       1,
				scmPerHost:  2,
				scmFailures: mockFailureMap(0),
				nvmePerHost: 2,
			}),
			expPrintStr: `
Errors:
  Hosts Error                
  ----- -----                
  host1 /mnt/1 format failed 

-----
host1
-----
SCM Mount Format Result 
--------- ------------- 
/mnt/2    CTL_SUCCESS   

NVMe PCI Format Result 
-------- ------------- 
2        CTL_SUCCESS   

`,
		},
		"2 SCM, 2 NVMe; second NVMe fails": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:        1,
				scmPerHost:   2,
				nvmePerHost:  2,
				nvmeFailures: mockFailureMap(1),
			}),
			expPrintStr: `
Errors:
  Hosts Error                       
  ----- -----                       
  host1 NVMe device 2 format failed 

-----
host1
-----
SCM Mount Format Result 
--------- ------------- 
/mnt/1    CTL_SUCCESS   
/mnt/2    CTL_SUCCESS   

NVMe PCI Format Result 
-------- ------------- 
1        CTL_SUCCESS   

`,
		},
		"2 SCM, 2 NVMe": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:       1,
				scmPerHost:  2,
				nvmePerHost: 2,
			}),
			expPrintStr: `

-----
host1
-----
SCM Mount Format Result 
--------- ------------- 
/mnt/1    CTL_SUCCESS   
/mnt/2    CTL_SUCCESS   

NVMe PCI Format Result 
-------- ------------- 
1        CTL_SUCCESS   
2        CTL_SUCCESS   

`,
		},
		"2 Hosts, 2 SCM, 2 NVMe; first SCM fails": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:       2,
				scmPerHost:  2,
				scmFailures: mockFailureMap(0),
				nvmePerHost: 2,
			}),
			expPrintStr: `
Errors:
  Hosts     Error                
  -----     -----                
  host[1-2] /mnt/1 format failed 

---------
host[1-2]
---------
SCM Mount Format Result 
--------- ------------- 
/mnt/2    CTL_SUCCESS   

NVMe PCI Format Result 
-------- ------------- 
2        CTL_SUCCESS   

`,
		},
		"2 Hosts, 2 SCM, 2 NVMe": {
			resp: mockFormatResp(t, mockFormatConf{
				hosts:       2,
				scmPerHost:  2,
				nvmePerHost: 2,
			}),
			expPrintStr: `

---------
host[1-2]
---------
SCM Mount Format Result 
--------- ------------- 
/mnt/1    CTL_SUCCESS   
/mnt/2    CTL_SUCCESS   

NVMe PCI Format Result 
-------- ------------- 
1        CTL_SUCCESS   
2        CTL_SUCCESS   

`,
		},
	} {
		t.Run(name, func(t *testing.T) {
			var bld strings.Builder
			if err := PrintResponseErrors(tc.resp, &bld); err != nil {
				t.Fatal(err)
			}
			if err := PrintStorageFormatMap(tc.resp.HostStorage, &bld, PrintWithVerboseOutput(true)); err != nil {
				t.Fatal(err)
			}

			if diff := cmp.Diff(strings.TrimLeft(tc.expPrintStr, "\n"), bld.String()); diff != "" {
				t.Fatalf("unexpected format string (-want, +got):\n%s\n", diff)
			}
		})
	}
}
