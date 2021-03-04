//
// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//! ProfCollect Binder service implementation.

use anyhow::{Context, Error, Result};
use binder::public_api::Result as BinderResult;
use binder::Status;
use profcollectd_aidl_interface::aidl::com::android::server::profcollect::IProfCollectd::IProfCollectd;
use std::ffi::CString;
use std::fs::{create_dir, read_to_string, remove_dir_all, remove_file, write};
use std::{
    str::FromStr,
    sync::{Mutex, MutexGuard},
};

use crate::config::{
    Config, CONFIG_FILE, OLD_REPORT_OUTPUT_FILE, PROFILE_OUTPUT_DIR, REPORT_OUTPUT_DIR,
    TRACE_OUTPUT_DIR,
};
use crate::report::pack_report;
use crate::scheduler::Scheduler;

fn err_to_binder_status(msg: Error) -> Status {
    let msg = CString::new(msg.to_string()).expect("Failed to convert to CString");
    Status::new_service_specific_error(1, Some(&msg))
}

pub struct ProfcollectdBinderService {
    lock: Mutex<Lock>,
}

struct Lock {
    config: Config,
    scheduler: Scheduler,
}

impl binder::Interface for ProfcollectdBinderService {}

impl IProfCollectd for ProfcollectdBinderService {
    fn schedule(&self) -> BinderResult<()> {
        let lock = &mut *self.lock();
        lock.scheduler
            .schedule_periodic(&lock.config)
            .context("Failed to schedule collection.")
            .map_err(err_to_binder_status)
    }
    fn terminate(&self) -> BinderResult<()> {
        self.lock()
            .scheduler
            .terminate_periodic()
            .context("Failed to terminate collection.")
            .map_err(err_to_binder_status)
    }
    fn trace_once(&self, tag: &str) -> BinderResult<()> {
        let lock = &mut *self.lock();
        lock.scheduler
            .one_shot(&lock.config, tag)
            .context("Failed to initiate an one-off trace.")
            .map_err(err_to_binder_status)
    }
    fn process(&self, blocking: bool) -> BinderResult<()> {
        let lock = &mut *self.lock();
        lock.scheduler
            .process(blocking)
            .context("Failed to process profiles.")
            .map_err(err_to_binder_status)
    }
    fn report(&self) -> BinderResult<()> {
        self.process(true)?;
        pack_report(&PROFILE_OUTPUT_DIR, &REPORT_OUTPUT_DIR)
            .context("Failed to create profile report.")
            .map_err(err_to_binder_status)
    }
    fn get_supported_provider(&self) -> BinderResult<String> {
        Ok(self.lock().scheduler.get_trace_provider_name().to_string())
    }
}

impl ProfcollectdBinderService {
    pub fn new() -> Result<Self> {
        let new_scheduler = Scheduler::new()?;
        let new_config = Config::from_env()?;

        let config_changed = read_to_string(*CONFIG_FILE)
            .ok()
            .and_then(|s| Config::from_str(&s).ok())
            .filter(|c| new_config == *c)
            .is_none();

        if config_changed {
            log::info!("Config change detected, clearing traces.");
            remove_dir_all(*PROFILE_OUTPUT_DIR)?;
            remove_dir_all(*TRACE_OUTPUT_DIR)?;
            create_dir(*PROFILE_OUTPUT_DIR)?;
            create_dir(*TRACE_OUTPUT_DIR)?;

            // Remove the report file in the old output location.
            // TODO: Remove this after all devices have updated to the new profcollect.
            if OLD_REPORT_OUTPUT_FILE.exists() {
                remove_file(*OLD_REPORT_OUTPUT_FILE)?;
            }

            write(*CONFIG_FILE, &new_config.to_string())?;
        }

        Ok(ProfcollectdBinderService {
            lock: Mutex::new(Lock { scheduler: new_scheduler, config: new_config }),
        })
    }

    fn lock(&self) -> MutexGuard<Lock> {
        self.lock.lock().unwrap()
    }
}
