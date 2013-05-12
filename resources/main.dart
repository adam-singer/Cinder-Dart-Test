library helloworld;

get _printClosure => (s) {
  try {
    log(s);
  } catch (_) {
    throw(s);
  }
};

void log(String what) native "Log";

void main() {
  print("hello world");
}