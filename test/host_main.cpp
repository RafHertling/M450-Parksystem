// Startet einen Sketch (setup/loop) auf dem PC
#include <unistd.h>
void setup();
void loop();
int main() {
  setup();
  for (;;) {
    loop();
    usleep(500);
  }
}
