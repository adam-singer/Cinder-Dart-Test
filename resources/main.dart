
void setup() {
}

get _printClosure => (s) {
  try {
    log(s);
  } catch (_) {
    throw(s);
  }
};

void log(String what) native "Log";

void circleColor( var color ) native "CircleColor";
void circleSegments(int count) native "CircleSegments";


void main() {
  print("hello dart.");
  
  var answer = 23 * 4 / 2;
  print( "the answer is $answer" );
  
  // circleColor( [1, 0, 0, 1] ); // won't work unless I handle integers in native CircleColor
  circleColor( [1.0, 0.2, 0.7, 1.0] );
  circleSegments( 5 );
}