//
//  kern_start.cpp
//  SnapshotIsKill
//
//  Copyright © 2021 flagers. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_file.hpp>
#include <Headers/kern_api.hpp>

#include <libkern/c++/OSUnserialize.h>

// Boot args.
static const char *bootargOff[] {
    "-sskilloff"
};
static const char *bootargDebug[] {
    "-sskilldbg"
};
static const char *bootargBeta[] {
    "-sskillbeta"
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

static const char *plistHeader {
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
    "<plist version=\"1.0\">\n"
};

static const char *plistFooter {
    "\n</plist>\n"
};

#define kUpdateOptionsPlist "/macOS Install Data/UpdateOptions.plist"

mach_vm_address_t orgVfsSwitchRoot {};
int wrapVfsSwitchRoot(const char *incoming_vol_old_path, const char *outgoing_vol_new_path, uint32_t flags) {
    uint8_t *buffer {nullptr};
    size_t bufferSize {0};
    if (NULL != (buffer = FileIO::readFileToBuffer(kUpdateOptionsPlist, bufferSize))) {
        OSObject *parsedXml = NULL;
        if (NULL != (parsedXml = OSUnserializeXML((const char *)buffer, bufferSize))) {
            OSDictionary *updateOptionsDict = NULL;
            updateOptionsDict = OSDynamicCast(OSDictionary, parsedXml);
            if (updateOptionsDict->setObject("DoNotSeal", kOSBooleanTrue)) {
                OSSerialize *serializer = OSSerialize::withCapacity(0x20000);
                serializer->addString(plistHeader);
                updateOptionsDict->serialize(serializer);
                serializer->addString(plistFooter);
                FileIO::writeBufferToFile(kUpdateOptionsPlist, serializer->text(), serializer->getLength());
            }
        }
    }
    
    if (buffer) { Buffer::deleter(buffer); }
    
    return FunctionCast(wrapVfsSwitchRoot, orgVfsSwitchRoot)(incoming_vol_old_path, outgoing_vol_new_path, flags);
};

// Plugin configuration.
PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery | LiluAPI::AllowSafeMode,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::BigSur,
    KernelVersion::Monterey,
    []() {
        lilu.onPatcherLoad([](void *user, KernelPatcher &patcher) {
            KernelPatcher::RouteRequest requests[] {
                KernelPatcher::RouteRequest("_fs_snapshot", wrapFsSnapshot, orgFsSnapshot),
                KernelPatcher::RouteRequest("_vfs_switch_root", wrapVfsSwitchRoot, orgVfsSwitchRoot)
            };
            
            if (!patcher.routeMultipleLong(KernelPatcher::KernelID, requests, arrsize(requests))) { PANIC("sskill", "failed to route"); };
        });
    }
};
