#include <math.h>
#include <iostream>

#include "SDL2/SDL.h"

void audio_callback(void* userdata, Uint8* stream, int len) {}

int main() {
  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    std::cerr << "Unable to initialize SDL: " << SDL_GetError() << std::endl;
    return EXIT_FAILURE;
  }

  SDL_AudioSpec want, have;
  SDL_memset(&want, 0, sizeof(want));
  want.freq = 48000;
  want.format = AUDIO_F32;
  want.channels = 1;
  want.samples = 4096;
  want.callback = nullptr;

  SDL_AudioDeviceID audio_device =
      SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (audio_device == 0) {
    std::cerr << "Failed to open audio device " << SDL_GetError() << std::endl;
    return EXIT_FAILURE;
  }

  float samples[40960];

  for (int i = 0; i < 40960; i++) {
    samples[i] = 10 * sin(261.6 * 2 * M_PI * float(i) / 48000);
  }

  SDL_QueueAudio(audio_device, &samples, sizeof(samples));
  SDL_PauseAudioDevice(audio_device, 0);

  SDL_Delay(5000);
  SDL_Quit();
}
