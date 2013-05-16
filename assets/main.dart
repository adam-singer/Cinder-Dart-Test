void setup() {}

void log( String what ) native "Log";
void toCinder( var obj ) native "SubmitToCinder";

get _printClosure => (s) {
  try {
    log(s);
  } catch (_) {
    throw(s);
  }
};

void main() {
  print("hello cinder, dart out.");
  
  var m = {
    'a' : 1,
    'b' : 2,
    'color' : [1, 0, 0, 1],
    'segments' : 12
  };
  // print( 'm: $m' );

  toCinder( m );
}