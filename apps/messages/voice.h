#ifndef BEPIO_MESSAGES_VOICE_H_
#define BEPIO_MESSAGES_VOICE_H_

/* Queue text for synthesis on the audio worker; no PCM is retained. */
int bepio_messages_voice_play(const char *text);

#endif
