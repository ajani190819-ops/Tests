/* ============================================================================
 *  Compile-time vtable slot assertions
 *
 *  COM dispatches by slot position, so a hand-written vtable that drops or
 *  reorders a method silently calls a different function with the wrong
 *  arguments -- it compiles, links, and then corrupts state or fails quietly.
 *  That is exactly how this app shipped with four broken interfaces. These
 *  assertions turn the whole class of bug into a build error.
 * ============================================================================ */
#define VTBL_SLOT(VtblType, field) \
    ((int)(offsetof(VtblType, field) / sizeof(void*)))

_Static_assert(VTBL_SLOT(IMMDeviceEnumeratorVtbl, EnumAudioEndpoints) == 3,      "IMMDeviceEnumerator::EnumAudioEndpoints must be slot 3");
_Static_assert(VTBL_SLOT(IMMDeviceEnumeratorVtbl, GetDefaultAudioEndpoint) == 4, "IMMDeviceEnumerator::GetDefaultAudioEndpoint must be slot 4");
_Static_assert(VTBL_SLOT(IMMDeviceVtbl, Activate) == 3,                          "IMMDevice::Activate must be slot 3");
_Static_assert(VTBL_SLOT(IMMDeviceVtbl, OpenPropertyStore) == 4,                 "IMMDevice::OpenPropertyStore must be slot 4");
_Static_assert(VTBL_SLOT(IMMDeviceVtbl, GetId) == 5,                             "IMMDevice::GetId must be slot 5");
_Static_assert(VTBL_SLOT(IMMDeviceVtbl, GetState) == 6,                          "IMMDevice::GetState must be slot 6");
_Static_assert(VTBL_SLOT(IMMDeviceCollectionVtbl, GetCount) == 3,                "IMMDeviceCollection::GetCount must be slot 3");
_Static_assert(VTBL_SLOT(IMMDeviceCollectionVtbl, Item) == 4,                    "IMMDeviceCollection::Item must be slot 4");
_Static_assert(VTBL_SLOT(IAudioEndpointVolumeVtbl, SetMasterVolumeLevelScalar) == 7,  "IAudioEndpointVolume::SetMasterVolumeLevelScalar must be slot 7");
_Static_assert(VTBL_SLOT(IAudioEndpointVolumeVtbl, GetMasterVolumeLevelScalar) == 9,  "IAudioEndpointVolume::GetMasterVolumeLevelScalar must be slot 9");
_Static_assert(VTBL_SLOT(IAudioEndpointVolumeVtbl, SetMute) == 14,               "IAudioEndpointVolume::SetMute must be slot 14");
_Static_assert(VTBL_SLOT(IAudioEndpointVolumeVtbl, GetMute) == 15,               "IAudioEndpointVolume::GetMute must be slot 15");
_Static_assert(VTBL_SLOT(IAudioSessionManager2Vtbl, GetSessionEnumerator) == 5,  "IAudioSessionManager2::GetSessionEnumerator must be slot 5");
_Static_assert(VTBL_SLOT(IAudioSessionEnumeratorVtbl, GetCount) == 3,            "IAudioSessionEnumerator::GetCount must be slot 3");
_Static_assert(VTBL_SLOT(IAudioSessionEnumeratorVtbl, GetSession) == 4,          "IAudioSessionEnumerator::GetSession must be slot 4");
_Static_assert(VTBL_SLOT(IAudioSessionControlVtbl, GetState) == 3,               "IAudioSessionControl::GetState must be slot 3");
_Static_assert(VTBL_SLOT(IAudioSessionControlVtbl, GetDisplayName) == 4,         "IAudioSessionControl::GetDisplayName must be slot 4");
_Static_assert(VTBL_SLOT(IAudioSessionControl2Vtbl, GetSessionIdentifier) == 12, "IAudioSessionControl2::GetSessionIdentifier must be slot 12");
_Static_assert(VTBL_SLOT(IAudioSessionControl2Vtbl, GetProcessId) == 14,         "IAudioSessionControl2::GetProcessId must be slot 14");
_Static_assert(VTBL_SLOT(IAudioSessionControl2Vtbl, IsSystemSoundsSession) == 15,"IAudioSessionControl2::IsSystemSoundsSession must be slot 15");
_Static_assert(VTBL_SLOT(ISimpleAudioVolumeVtbl, SetMasterVolume) == 3,          "ISimpleAudioVolume::SetMasterVolume must be slot 3");
_Static_assert(VTBL_SLOT(ISimpleAudioVolumeVtbl, GetMasterVolume) == 4,          "ISimpleAudioVolume::GetMasterVolume must be slot 4");
_Static_assert(VTBL_SLOT(ISimpleAudioVolumeVtbl, SetMute) == 5,                  "ISimpleAudioVolume::SetMute must be slot 5");
_Static_assert(VTBL_SLOT(ISimpleAudioVolumeVtbl, GetMute) == 6,                  "ISimpleAudioVolume::GetMute must be slot 6");
_Static_assert(VTBL_SLOT(IPolicyConfigVtbl, SetDefaultEndpoint) == 13,           "IPolicyConfig::SetDefaultEndpoint must be slot 13");

typedef void (__stdcall *ReleaseFn)(void*);

/* --- Helper macro for COM calls --- *//* --- Helper macro for COM calls --- */
#define CALL_VTBL0(iface, idx) ((iface)->lpVtbl->idx(iface))
#define CALL_VTBL1(iface, idx, a1) ((iface)->lpVtbl->idx(iface, a1))
#define CALL_VTBL2(iface, idx, a1, a2) ((iface)->lpVtbl->idx(iface, a1, a2))
#define CALL_VTBL3(iface, idx, a1, a2, a3) ((iface)->lpVtbl->idx(iface, a1, a2, a3))
#define CALL_VTBL4(iface, idx, a1, a2, a3, a4) ((iface)->lpVtbl->idx(iface, a1, a2, a3, a4))
#define CALL_VTBL5(iface, idx, a1, a2, a3, a4, a5) ((iface)->lpVtbl->idx(iface, a1, a2, a3, a4, a5))

