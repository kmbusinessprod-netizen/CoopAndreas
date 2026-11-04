#pragma once
class CPatch
{
public:
	static void TemporaryPatches();
	static void RevertTemporaryPatches();
	static void RevertTemporaryPatchesForHost();
	static void PatchFramerate();
	static void EnforceFrameLimiter();
	static void CaptureGameProcessHook();
	static void RestoreGameProcessHook();
	static void ApplyPatches();
};

