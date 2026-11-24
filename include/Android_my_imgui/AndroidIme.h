#pragma once
#include <string>

namespace AndroidIme {
void Init();
void Poll();
void EndFrame();
float KeyboardHeight();
std::string ConsumeError();
void Shutdown();
int RunHost();
int RunApkHost();
int Diagnose(bool temporary_apk = false);
} // namespace AndroidIme
