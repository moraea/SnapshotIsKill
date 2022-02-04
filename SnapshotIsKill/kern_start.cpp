//
//  kern_start.cpp
//  SnapshotIsKill
//
//  Copyright © 2021 flagers. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_api.hpp>

// Boot args.
static const char *bootargOff[] {
    "-snapshotiskilloff"
};
static const char *bootargDebug[] {
    "-snapshotiskilldbg"
};
static const char *bootargBeta[] {
    "-snapshotiskillbeta"
};

#define SNAPSHOT_OP_CREATE 0x01
#define SNAPSHOT_OP_DELETE 0x02
#define SNAPSHOT_OP_RENAME 0x03
#define SNAPSHOT_OP_MOUNT  0x04
#define SNAPSHOT_OP_REVERT 0x05
#define SNAPSHOT_OP_ROOT   0x06

mach_vm_address_t orgFsSnapshot {};
int wrapFsSnapshot(SInt64 a1, SInt64 uap_op) {
    switch (uap_op) {
        case SNAPSHOT_OP_CREATE:
        case SNAPSHOT_OP_ROOT:
            return 0;
        default:
            return FunctionCast(wrapFsSnapshot, orgFsSnapshot)(a1, uap_op);
    }
};

// Plugin configuration.
PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::BigSur,
    KernelVersion::Monterey,
    []() {
        LiluAPI::Error error = lilu.onPatcherLoad([](void *user, KernelPatcher &patcher) {
            KernelPatcher::RouteRequest requests[] {
                KernelPatcher::RouteRequest("_fs_snapshot", wrapFsSnapshot, orgFsSnapshot)
            };
            
            if (!patcher.routeMultipleLong(KernelPatcher::KernelID, requests, 1)) { PANIC("sskill", "failed to route"); };
        });
    }
};
