#
# Copyright (C) 2024 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import unittest
from unittest import mock
from config_builder import build_default_config
from command import ProfilerCommand


DEFAULT_CONFIG_9000_DUR_MS = f'''\
<<EOF

buffers: {{
  size_kb: 4096
  fill_policy: RING_BUFFER
}}
buffers {{
  size_kb: 4096
  fill_policy: RING_BUFFER
}}
buffers: {{
  size_kb: 260096
  fill_policy: RING_BUFFER
}}

data_sources: {{
  config {{
    name: "linux.process_stats"
    process_stats_config {{
      scan_all_processes_on_start: true
    }}
  }}
}}

data_sources: {{
  config {{
    name: "android.log"
    android_log_config {{
    }}
  }}
}}

data_sources {{
  config {{
    name: "android.packages_list"
  }}
}}

data_sources: {{
  config {{
    name: "linux.sys_stats"
    target_buffer: 1
    sys_stats_config {{
      stat_period_ms: 500
      stat_counters: STAT_CPU_TIMES
      stat_counters: STAT_FORK_COUNT
      meminfo_period_ms: 1000
      meminfo_counters: MEMINFO_ACTIVE_ANON
      meminfo_counters: MEMINFO_ACTIVE_FILE
      meminfo_counters: MEMINFO_INACTIVE_ANON
      meminfo_counters: MEMINFO_INACTIVE_FILE
      meminfo_counters: MEMINFO_KERNEL_STACK
      meminfo_counters: MEMINFO_MLOCKED
      meminfo_counters: MEMINFO_SHMEM
      meminfo_counters: MEMINFO_SLAB
      meminfo_counters: MEMINFO_SLAB_UNRECLAIMABLE
      meminfo_counters: MEMINFO_VMALLOC_USED
      meminfo_counters: MEMINFO_MEM_FREE
      meminfo_counters: MEMINFO_SWAP_FREE
      vmstat_period_ms: 1000
      vmstat_counters: VMSTAT_PGFAULT
      vmstat_counters: VMSTAT_PGMAJFAULT
      vmstat_counters: VMSTAT_PGFREE
      vmstat_counters: VMSTAT_PGPGIN
      vmstat_counters: VMSTAT_PGPGOUT
      vmstat_counters: VMSTAT_PSWPIN
      vmstat_counters: VMSTAT_PSWPOUT
      vmstat_counters: VMSTAT_PGSCAN_DIRECT
      vmstat_counters: VMSTAT_PGSTEAL_DIRECT
      vmstat_counters: VMSTAT_PGSCAN_KSWAPD
      vmstat_counters: VMSTAT_PGSTEAL_KSWAPD
      vmstat_counters: VMSTAT_WORKINGSET_REFAULT
      # Below field not available on < Android SC-V2 releases.
      cpufreq_period_ms: 500
    }}
  }}
}}

data_sources: {{
  config {{
    name: "android.surfaceflinger.frametimeline"
    target_buffer: 2
  }}
}}

data_sources: {{
  config {{
    name: "linux.ftrace"
    target_buffer: 2
    ftrace_config {{
      ftrace_events: "dmabuf_heap/dma_heap_stat"
      ftrace_events: "ftrace/print"
      ftrace_events: "gpu_mem/gpu_mem_total"
      ftrace_events: "ion/ion_stat"
      ftrace_events: "kmem/ion_heap_grow"
      ftrace_events: "kmem/ion_heap_shrink"
      ftrace_events: "kmem/rss_stat"
      ftrace_events: "lowmemorykiller/lowmemory_kill"
      ftrace_events: "mm_event/mm_event_record"
      ftrace_events: "oom/mark_victim"
      ftrace_events: "oom/oom_score_adj_update"
      ftrace_events: "power/cpu_frequency"
      ftrace_events: "power/cpu_idle"
      ftrace_events: "power/gpu_frequency"
      ftrace_events: "power/suspend_resume"
      ftrace_events: "power/wakeup_source_activate"
      ftrace_events: "power/wakeup_source_deactivate"
      ftrace_events: "sched/sched_blocked_reason"
      ftrace_events: "sched/sched_process_exit"
      ftrace_events: "sched/sched_process_free"
      ftrace_events: "sched/sched_switch"
      ftrace_events: "sched/sched_wakeup"
      ftrace_events: "sched/sched_wakeup_new"
      ftrace_events: "sched/sched_waking"
      ftrace_events: "task/task_newtask"
      ftrace_events: "task/task_rename"
      ftrace_events: "vmscan/*"
      ftrace_events: "workqueue/*"
      atrace_categories: "aidl"
      atrace_categories: "am"
      atrace_categories: "dalvik"
      atrace_categories: "binder_lock"
      atrace_categories: "binder_driver"
      atrace_categories: "bionic"
      atrace_categories: "camera"
      atrace_categories: "disk"
      atrace_categories: "freq"
      atrace_categories: "idle"
      atrace_categories: "gfx"
      atrace_categories: "hal"
      atrace_categories: "input"
      atrace_categories: "pm"
      atrace_categories: "power"
      atrace_categories: "res"
      atrace_categories: "rro"
      atrace_categories: "sched"
      atrace_categories: "sm"
      atrace_categories: "ss"
      atrace_categories: "thermal"
      atrace_categories: "video"
      atrace_categories: "view"
      atrace_categories: "wm"
      atrace_apps: "lmkd"
      atrace_apps: "system_server"
      atrace_apps: "com.android.systemui"
      atrace_apps: "com.google.android.gms"
      atrace_apps: "com.google.android.gms.persistent"
      atrace_apps: "android:ui"
      atrace_apps: "com.google.android.apps.maps"
      atrace_apps: "*"
      buffer_size_kb: 16384
      drain_period_ms: 150
      symbolize_ksyms: true
    }}
  }}
}}
duration_ms: 9000
write_into_file: true
file_write_period_ms: 5000
max_file_size_bytes: 100000000000
flush_period_ms: 5000
incremental_state_config {{
  clear_period_ms: 5000
}}

EOF'''

DEFAULT_CONFIG_EXCLUDED_FTRACE_EVENTS = f'''\
<<EOF

buffers: {{
  size_kb: 4096
  fill_policy: RING_BUFFER
}}
buffers {{
  size_kb: 4096
  fill_policy: RING_BUFFER
}}
buffers: {{
  size_kb: 260096
  fill_policy: RING_BUFFER
}}

data_sources: {{
  config {{
    name: "linux.process_stats"
    process_stats_config {{
      scan_all_processes_on_start: true
    }}
  }}
}}

data_sources: {{
  config {{
    name: "android.log"
    android_log_config {{
    }}
  }}
}}

data_sources {{
  config {{
    name: "android.packages_list"
  }}
}}

data_sources: {{
  config {{
    name: "linux.sys_stats"
    target_buffer: 1
    sys_stats_config {{
      stat_period_ms: 500
      stat_counters: STAT_CPU_TIMES
      stat_counters: STAT_FORK_COUNT
      meminfo_period_ms: 1000
      meminfo_counters: MEMINFO_ACTIVE_ANON
      meminfo_counters: MEMINFO_ACTIVE_FILE
      meminfo_counters: MEMINFO_INACTIVE_ANON
      meminfo_counters: MEMINFO_INACTIVE_FILE
      meminfo_counters: MEMINFO_KERNEL_STACK
      meminfo_counters: MEMINFO_MLOCKED
      meminfo_counters: MEMINFO_SHMEM
      meminfo_counters: MEMINFO_SLAB
      meminfo_counters: MEMINFO_SLAB_UNRECLAIMABLE
      meminfo_counters: MEMINFO_VMALLOC_USED
      meminfo_counters: MEMINFO_MEM_FREE
      meminfo_counters: MEMINFO_SWAP_FREE
      vmstat_period_ms: 1000
      vmstat_counters: VMSTAT_PGFAULT
      vmstat_counters: VMSTAT_PGMAJFAULT
      vmstat_counters: VMSTAT_PGFREE
      vmstat_counters: VMSTAT_PGPGIN
      vmstat_counters: VMSTAT_PGPGOUT
      vmstat_counters: VMSTAT_PSWPIN
      vmstat_counters: VMSTAT_PSWPOUT
      vmstat_counters: VMSTAT_PGSCAN_DIRECT
      vmstat_counters: VMSTAT_PGSTEAL_DIRECT
      vmstat_counters: VMSTAT_PGSCAN_KSWAPD
      vmstat_counters: VMSTAT_PGSTEAL_KSWAPD
      vmstat_counters: VMSTAT_WORKINGSET_REFAULT
      # Below field not available on < Android SC-V2 releases.
      cpufreq_period_ms: 500
    }}
  }}
}}

data_sources: {{
  config {{
    name: "android.surfaceflinger.frametimeline"
    target_buffer: 2
  }}
}}

data_sources: {{
  config {{
    name: "linux.ftrace"
    target_buffer: 2
    ftrace_config {{
      ftrace_events: "dmabuf_heap/dma_heap_stat"
      ftrace_events: "ftrace/print"
      ftrace_events: "gpu_mem/gpu_mem_total"
      ftrace_events: "ion/ion_stat"
      ftrace_events: "kmem/ion_heap_grow"
      ftrace_events: "kmem/ion_heap_shrink"
      ftrace_events: "kmem/rss_stat"
      ftrace_events: "lowmemorykiller/lowmemory_kill"
      ftrace_events: "oom/mark_victim"
      ftrace_events: "oom/oom_score_adj_update"
      ftrace_events: "power/cpu_frequency"
      ftrace_events: "power/cpu_idle"
      ftrace_events: "power/gpu_frequency"
      ftrace_events: "power/wakeup_source_activate"
      ftrace_events: "power/wakeup_source_deactivate"
      ftrace_events: "sched/sched_blocked_reason"
      ftrace_events: "sched/sched_process_exit"
      ftrace_events: "sched/sched_process_free"
      ftrace_events: "sched/sched_switch"
      ftrace_events: "sched/sched_wakeup"
      ftrace_events: "sched/sched_wakeup_new"
      ftrace_events: "sched/sched_waking"
      ftrace_events: "task/task_newtask"
      ftrace_events: "task/task_rename"
      ftrace_events: "vmscan/*"
      ftrace_events: "workqueue/*"
      atrace_categories: "aidl"
      atrace_categories: "am"
      atrace_categories: "dalvik"
      atrace_categories: "binder_lock"
      atrace_categories: "binder_driver"
      atrace_categories: "bionic"
      atrace_categories: "camera"
      atrace_categories: "disk"
      atrace_categories: "freq"
      atrace_categories: "idle"
      atrace_categories: "gfx"
      atrace_categories: "hal"
      atrace_categories: "input"
      atrace_categories: "pm"
      atrace_categories: "power"
      atrace_categories: "res"
      atrace_categories: "rro"
      atrace_categories: "sched"
      atrace_categories: "sm"
      atrace_categories: "ss"
      atrace_categories: "thermal"
      atrace_categories: "video"
      atrace_categories: "view"
      atrace_categories: "wm"
      atrace_apps: "lmkd"
      atrace_apps: "system_server"
      atrace_apps: "com.android.systemui"
      atrace_apps: "com.google.android.gms"
      atrace_apps: "com.google.android.gms.persistent"
      atrace_apps: "android:ui"
      atrace_apps: "com.google.android.apps.maps"
      atrace_apps: "*"
      buffer_size_kb: 16384
      drain_period_ms: 150
      symbolize_ksyms: true
    }}
  }}
}}
duration_ms: 10000
write_into_file: true
file_write_period_ms: 5000
max_file_size_bytes: 100000000000
flush_period_ms: 5000
incremental_state_config {{
  clear_period_ms: 5000
}}

EOF'''

DEFAULT_CONFIG_INCLUDED_FTRACE_EVENTS = f'''\
<<EOF

buffers: {{
  size_kb: 4096
  fill_policy: RING_BUFFER
}}
buffers {{
  size_kb: 4096
  fill_policy: RING_BUFFER
}}
buffers: {{
  size_kb: 260096
  fill_policy: RING_BUFFER
}}

data_sources: {{
  config {{
    name: "linux.process_stats"
    process_stats_config {{
      scan_all_processes_on_start: true
    }}
  }}
}}

data_sources: {{
  config {{
    name: "android.log"
    android_log_config {{
    }}
  }}
}}

data_sources {{
  config {{
    name: "android.packages_list"
  }}
}}

data_sources: {{
  config {{
    name: "linux.sys_stats"
    target_buffer: 1
    sys_stats_config {{
      stat_period_ms: 500
      stat_counters: STAT_CPU_TIMES
      stat_counters: STAT_FORK_COUNT
      meminfo_period_ms: 1000
      meminfo_counters: MEMINFO_ACTIVE_ANON
      meminfo_counters: MEMINFO_ACTIVE_FILE
      meminfo_counters: MEMINFO_INACTIVE_ANON
      meminfo_counters: MEMINFO_INACTIVE_FILE
      meminfo_counters: MEMINFO_KERNEL_STACK
      meminfo_counters: MEMINFO_MLOCKED
      meminfo_counters: MEMINFO_SHMEM
      meminfo_counters: MEMINFO_SLAB
      meminfo_counters: MEMINFO_SLAB_UNRECLAIMABLE
      meminfo_counters: MEMINFO_VMALLOC_USED
      meminfo_counters: MEMINFO_MEM_FREE
      meminfo_counters: MEMINFO_SWAP_FREE
      vmstat_period_ms: 1000
      vmstat_counters: VMSTAT_PGFAULT
      vmstat_counters: VMSTAT_PGMAJFAULT
      vmstat_counters: VMSTAT_PGFREE
      vmstat_counters: VMSTAT_PGPGIN
      vmstat_counters: VMSTAT_PGPGOUT
      vmstat_counters: VMSTAT_PSWPIN
      vmstat_counters: VMSTAT_PSWPOUT
      vmstat_counters: VMSTAT_PGSCAN_DIRECT
      vmstat_counters: VMSTAT_PGSTEAL_DIRECT
      vmstat_counters: VMSTAT_PGSCAN_KSWAPD
      vmstat_counters: VMSTAT_PGSTEAL_KSWAPD
      vmstat_counters: VMSTAT_WORKINGSET_REFAULT
      # Below field not available on < Android SC-V2 releases.
      cpufreq_period_ms: 500
    }}
  }}
}}

data_sources: {{
  config {{
    name: "android.surfaceflinger.frametimeline"
    target_buffer: 2
  }}
}}

data_sources: {{
  config {{
    name: "linux.ftrace"
    target_buffer: 2
    ftrace_config {{
      ftrace_events: "dmabuf_heap/dma_heap_stat"
      ftrace_events: "ftrace/print"
      ftrace_events: "gpu_mem/gpu_mem_total"
      ftrace_events: "ion/ion_stat"
      ftrace_events: "kmem/ion_heap_grow"
      ftrace_events: "kmem/ion_heap_shrink"
      ftrace_events: "kmem/rss_stat"
      ftrace_events: "lowmemorykiller/lowmemory_kill"
      ftrace_events: "mm_event/mm_event_record"
      ftrace_events: "oom/mark_victim"
      ftrace_events: "oom/oom_score_adj_update"
      ftrace_events: "power/cpu_frequency"
      ftrace_events: "power/cpu_idle"
      ftrace_events: "power/gpu_frequency"
      ftrace_events: "power/suspend_resume"
      ftrace_events: "power/wakeup_source_activate"
      ftrace_events: "power/wakeup_source_deactivate"
      ftrace_events: "sched/sched_blocked_reason"
      ftrace_events: "sched/sched_process_exit"
      ftrace_events: "sched/sched_process_free"
      ftrace_events: "sched/sched_switch"
      ftrace_events: "sched/sched_wakeup"
      ftrace_events: "sched/sched_wakeup_new"
      ftrace_events: "sched/sched_waking"
      ftrace_events: "task/task_newtask"
      ftrace_events: "task/task_rename"
      ftrace_events: "vmscan/*"
      ftrace_events: "workqueue/*"
      ftrace_events: "mock_ftrace_event1"
      ftrace_events: "mock_ftrace_event2"
      atrace_categories: "aidl"
      atrace_categories: "am"
      atrace_categories: "dalvik"
      atrace_categories: "binder_lock"
      atrace_categories: "binder_driver"
      atrace_categories: "bionic"
      atrace_categories: "camera"
      atrace_categories: "disk"
      atrace_categories: "freq"
      atrace_categories: "idle"
      atrace_categories: "gfx"
      atrace_categories: "hal"
      atrace_categories: "input"
      atrace_categories: "pm"
      atrace_categories: "power"
      atrace_categories: "res"
      atrace_categories: "rro"
      atrace_categories: "sched"
      atrace_categories: "sm"
      atrace_categories: "ss"
      atrace_categories: "thermal"
      atrace_categories: "video"
      atrace_categories: "view"
      atrace_categories: "wm"
      atrace_apps: "lmkd"
      atrace_apps: "system_server"
      atrace_apps: "com.android.systemui"
      atrace_apps: "com.google.android.gms"
      atrace_apps: "com.google.android.gms.persistent"
      atrace_apps: "android:ui"
      atrace_apps: "com.google.android.apps.maps"
      atrace_apps: "*"
      buffer_size_kb: 16384
      drain_period_ms: 150
      symbolize_ksyms: true
    }}
  }}
}}
duration_ms: 10000
write_into_file: true
file_write_period_ms: 5000
max_file_size_bytes: 100000000000
flush_period_ms: 5000
incremental_state_config {{
  clear_period_ms: 5000
}}

EOF'''


class ConfigBuilderUnitTest(unittest.TestCase):

  def test_config_builder_setting_valid_dur_ms(self):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.dur_ms = 9000
    mock_command.excluded_ftrace_events = None
    mock_command.included_ftrace_events = None

    config, error = build_default_config(mock_command)

    self.assertEqual(error, None)
    self.assertEqual(config, DEFAULT_CONFIG_9000_DUR_MS)

  def test_config_builder_setting_invalid_dur_ms(self):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.dur_ms = None
    mock_command.excluded_ftrace_events = None
    mock_command.included_ftrace_events = None

    with self.assertRaises(ValueError) as e:
      build_default_config(mock_command)

    self.assertEqual(str(e.exception), ("Cannot create config because a valid"
                                        " dur_ms was not set."))

  def test_config_builder_removing_valid_excluded_ftrace_events(self):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.dur_ms = 10000
    mock_command.excluded_ftrace_events = ["power/suspend_resume",
                                           "mm_event/mm_event_record"]
    mock_command.included_ftrace_events = None

    config, error = build_default_config(mock_command)

    self.assertEqual(error, None)
    self.assertEqual(config, DEFAULT_CONFIG_EXCLUDED_FTRACE_EVENTS)

  def test_config_builder_removing_invalid_excluded_ftrace_events(self):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.dur_ms = 10000
    mock_command.excluded_ftrace_events = ["invalid_ftrace_event"]
    mock_command.included_ftrace_events = None

    config, error = build_default_config(mock_command)

    self.assertEqual(config, None)
    self.assertNotEqual(error, None)
    self.assertEqual(error.message, ("Cannot remove ftrace event %s from config"
                                     " because it is not one of the config's"
                                     " ftrace events."
                                     % mock_command.excluded_ftrace_events[0]))
    self.assertEqual(error.suggestion, ("Please specify one of the following"
                                        " possible ftrace events:\n\t"
                                        " dmabuf_heap/dma_heap_stat\n\t"
                                        " ftrace/print\n\t"
                                        " gpu_mem/gpu_mem_total\n\t"
                                        " ion/ion_stat\n\t"
                                        " kmem/ion_heap_grow\n\t"
                                        " kmem/ion_heap_shrink\n\t"
                                        " kmem/rss_stat\n\t"
                                        " lowmemorykiller/lowmemory_kill\n\t"
                                        " mm_event/mm_event_record\n\t"
                                        " oom/mark_victim\n\t"
                                        " oom/oom_score_adj_update\n\t"
                                        " power/cpu_frequency\n\t"
                                        " power/cpu_idle\n\t"
                                        " power/gpu_frequency\n\t"
                                        " power/suspend_resume\n\t"
                                        " power/wakeup_source_activate\n\t"
                                        " power/wakeup_source_deactivate\n\t"
                                        " sched/sched_blocked_reason\n\t"
                                        " sched/sched_process_exit\n\t"
                                        " sched/sched_process_free\n\t"
                                        " sched/sched_switch\n\t"
                                        " sched/sched_wakeup\n\t"
                                        " sched/sched_wakeup_new\n\t"
                                        " sched/sched_waking\n\t"
                                        " task/task_newtask\n\t"
                                        " task/task_rename\n\t"
                                        " vmscan/*\n\t"
                                        " workqueue/*"))

  def test_config_builder_adding_valid_included_ftrace_events(self):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.dur_ms = 10000
    mock_command.excluded_ftrace_events = None
    mock_command.included_ftrace_events = ["mock_ftrace_event1",
                                           "mock_ftrace_event2"]

    config, error = build_default_config(mock_command)

    self.assertEqual(error, None)
    self.assertEqual(config, DEFAULT_CONFIG_INCLUDED_FTRACE_EVENTS)

  def test_config_builder_adding_invalid_included_ftrace_events(self):
    mock_command = mock.create_autospec(ProfilerCommand, instance=True)
    mock_command.dur_ms = 10000
    mock_command.excluded_ftrace_events = None
    mock_command.included_ftrace_events = ["power/suspend_resume"]

    config, error = build_default_config(mock_command)

    self.assertEqual(config, None)
    self.assertNotEqual(error, None)
    self.assertEqual(error.message, ("Cannot add ftrace event %s to config"
                                     " because it is already one of the"
                                     " config's ftrace events."
                                     % mock_command.included_ftrace_events[0]))
    self.assertEqual(error.suggestion, ("Please do not specify any of the"
                                        " following ftrace events that are"
                                        " already included:\n\t"
                                        " dmabuf_heap/dma_heap_stat\n\t"
                                        " ftrace/print\n\t"
                                        " gpu_mem/gpu_mem_total\n\t"
                                        " ion/ion_stat\n\t"
                                        " kmem/ion_heap_grow\n\t"
                                        " kmem/ion_heap_shrink\n\t"
                                        " kmem/rss_stat\n\t"
                                        " lowmemorykiller/lowmemory_kill\n\t"
                                        " mm_event/mm_event_record\n\t"
                                        " oom/mark_victim\n\t"
                                        " oom/oom_score_adj_update\n\t"
                                        " power/cpu_frequency\n\t"
                                        " power/cpu_idle\n\t"
                                        " power/gpu_frequency\n\t"
                                        " power/suspend_resume\n\t"
                                        " power/wakeup_source_activate\n\t"
                                        " power/wakeup_source_deactivate\n\t"
                                        " sched/sched_blocked_reason\n\t"
                                        " sched/sched_process_exit\n\t"
                                        " sched/sched_process_free\n\t"
                                        " sched/sched_switch\n\t"
                                        " sched/sched_wakeup\n\t"
                                        " sched/sched_wakeup_new\n\t"
                                        " sched/sched_waking\n\t"
                                        " task/task_newtask\n\t"
                                        " task/task_rename\n\t"
                                        " vmscan/*\n\t"
                                        " workqueue/*"))


if __name__ == '__main__':
  unittest.main()
