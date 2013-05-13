#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Utilities.h"

#include "dart_api.h"

const char* VM_FLAGS[] = {
	"--enable-checked-mode"
//	"--print-flags"
	// "--trace_isolates",
	// "--trace_natives",
//	 "--trace_compiler"
};

#define LOG_V ci::app::console() << __func__ << " | "
#define LOG_E LOG_V << __LINE__ << " | " << " *** ERROR *** : "

#define CHECK_RESULT(result)					\
if (Dart_IsError(result)) {						\
	*error = strdup(Dart_GetError(result));		\
	LOG_E << *error;							\
	Dart_ExitScope();							\
	Dart_ShutdownIsolate();						\
	return false;								\
}

struct FunctionLookup {
	const char* name;
	Dart_NativeFunction function;
};

const char* GetArgAsString(Dart_NativeArguments arguments, int idx) {
	char** error;
	Dart_Handle handle = Dart_GetNativeArgument( arguments, idx );
	CHECK_RESULT( handle );
	uint8_t* str;
	intptr_t length;
	CHECK_RESULT( Dart_StringLength( handle, &length ) );
	CHECK_RESULT( Dart_StringToUTF8( handle, &str, &length ) );
	str[length] = 0;
	return  const_cast<const char*>(reinterpret_cast<char*>(str));
}

void Log(Dart_NativeArguments arguments) {
	Dart_EnterScope();
	std::cout << GetArgAsString(arguments, 0) << std::endl;
	Dart_ExitScope();
}

FunctionLookup function_list[] = {
    {"Log", Log},
};

Dart_NativeFunction ResolveName(Dart_Handle name, int argc) {
	if (!Dart_IsString(name)) return NULL;
	Dart_NativeFunction result = NULL;
	Dart_EnterScope();
	const char* cname;
	char** error; // TODO: this should be in the check result macro
	CHECK_RESULT( Dart_StringToCString( name, &cname ) );
	for (int i = 0; function_list[i].name != NULL; ++i) {
		if (strcmp(function_list[i].name, cname) == 0) {
			result = function_list[i].function;
			break;
		}
	}
	Dart_ExitScope();
	return result;
}

using namespace ci;
using namespace ci::app;
using namespace std;

class DartTestApp : public AppNative {
  public:
	void setup();
	void update();
	void draw();

	static Dart_Isolate createIsolateAndSetup(const char* script_uri, const char* main, void* data, char** error);
	static Dart_Handle libraryTagHandler( Dart_LibraryTag tag, Dart_Handle library, Dart_Handle urlHandle );
	static Dart_Handle checkError( Dart_Handle handle );
	static void* openFileCallback(const char* name, bool write);
	static void readFileCallback(const uint8_t** data, intptr_t* fileLength, void* stream );
	static void writeFileCallback(const void* data, intptr_t length, void* file);
	static void closeFileCallback(void* file);

	void invoke( const char* function, int argc = 0, Dart_Handle* args = NULL );
};

void DartTestApp::setup()
{
	DataSourceRef script = loadResource( "main.dart" );

	// init dart and create Isolate
	LOG_V << "Setting VM Options" << endl;
	bool success = Dart_SetVMFlags(sizeof(VM_FLAGS) / sizeof(VM_FLAGS[0]), VM_FLAGS);
	assert( success );

	success = Dart_Initialize( createIsolateAndSetup, 0, 0, 0, openFileCallback, readFileCallback, writeFileCallback, closeFileCallback );
	assert( success );

	LOG_V << "Dart_Initialize complete." << endl;

	const char *scriptPath = script->getFilePath().c_str();
	char *error = NULL;
	Dart_Isolate isolate = createIsolateAndSetup( scriptPath, "main", NULL, &error );
	if( ! isolate ) {
		LOG_E << "could not create isolate: " << error << endl;
		assert( 0 );
	}
	assert( isolate == Dart_CurrentIsolate() );

	// load script

	Dart_EnterScope();

	Dart_Handle url = checkError( Dart_NewStringFromCString( scriptPath ) );
	string scriptContents = loadString( script );

	Dart_Handle source = checkError( Dart_NewStringFromCString( scriptContents.c_str() ) );
	checkError( Dart_LoadScript( url, source, 0, 0 ) );

	// apparently 'something' must be called before swapping in print,
	// else she blows up with: parser.cc:4996: error: expected: current_class().is_finalized()
	invoke( "setup" );

	Dart_Handle library = Dart_RootLibrary();
	if ( Dart_IsNull( library ) ) {
		LOG_E << "Unable to find root library" << endl;
		return;
	}

	// load in our custom _printCloser, which maps back to Log
	Dart_Handle corelib = checkError( Dart_LookupLibrary( Dart_NewStringFromCString( "dart:core" ) ) );
	Dart_Handle print = checkError( Dart_GetField( library, Dart_NewStringFromCString( "_printClosure" ) ) );
	checkError( Dart_SetField( corelib, Dart_NewStringFromCString( "_printClosure" ), print ) );

	checkError( Dart_SetNativeResolver( library, ResolveName ) );

	invoke( "main" );

	Dart_ExitScope();
    Dart_ExitIsolate();
}

Dart_Isolate DartTestApp::createIsolateAndSetup( const char* script_uri, const char* main, void* data, char** error )
{
	DataSourceRef snapshot = loadResource( "snapshot_gen.bin" );
	const uint8_t *snapshotData = (const uint8_t *)snapshot->getBuffer().getData();

	LOG_V << "Creating isolate " << script_uri << ", " << main << endl;
	Dart_Isolate isolate = Dart_CreateIsolate( script_uri, main, snapshotData, data, error );
	if ( isolate == NULL ) {
		LOG_E << "Couldn't create isolate: " << *error << endl;
		return NULL;
	}

	LOG_V << "Entering scope" << endl;
	Dart_EnterScope();

	// Set up the library tag handler for this isolate.
	LOG_V << "Setting up library tag handler" << endl;
	Dart_Handle result = Dart_SetLibraryTagHandler( libraryTagHandler );
	CHECK_RESULT( result );

	Dart_ExitScope();
	return isolate;
}

Dart_Handle DartTestApp::libraryTagHandler( Dart_LibraryTag tag, Dart_Handle library, Dart_Handle urlHandle )
{
	const char* url;
	Dart_StringToCString(urlHandle, &url);
	LOG_V << "url: " << url;
	
	if (tag == kCanonicalizeUrl) {
		return urlHandle;
	}

	LOG_E << "UNIMPLEMENTED: load library: " << url << endl;
	return NULL;
}

Dart_Handle DartTestApp::checkError( Dart_Handle handle ) {
	if( Dart_IsError( handle ) ) {
		LOG_E << "Unexpected Error Handle: " << Dart_GetError( handle ) << endl;
//		Dart_PropagateError(handle);
//		assert( 0 );
	}
	return handle;
}

void DartTestApp::invoke( const char* function, int argc, Dart_Handle* args )
{
	// Lookup the library of the root script.
	Dart_Handle library = Dart_RootLibrary();
	if ( Dart_IsNull( library ) ) {
		LOG_E << "Unable to find root library" << endl;
		return;
	}

	Dart_Handle nameHandle = Dart_NewStringFromCString( function );

	Dart_Handle result = Dart_Invoke( library, nameHandle, argc, args );

	if(Dart_IsError( result ) ) {
		const char* error = Dart_GetError(result);
		LOG_E << "Invoke '" << function << "' failed: " << error << endl;
		return;
	}

	// TODO: there was originally a note saying this probably isn't necessary.. try removing
	// Keep handling messages until the last active receive port is closed.
	result = Dart_RunLoop();
	if ( Dart_IsError( result ) ) {
		const char* error = Dart_GetError(result);
		LOG_E << "Dart_RunLoop: " << error << endl;
		return;
	}

	return;
}

void* DartTestApp::openFileCallback(const char* name, bool write)
{
	return fopen(name, write ? "w" : "r");
}

void DartTestApp::readFileCallback(const uint8_t** data, intptr_t* fileLength, void* stream )
{
	if (!stream) {
		*data = 0;
		*fileLength = 0;
	} else {
		FILE* file = reinterpret_cast<FILE*>(stream);

		// Get the file size.
		fseek(file, 0, SEEK_END);
		*fileLength = ftell(file);
		rewind(file);

		// Allocate data buffer.
		*data = new uint8_t[*fileLength];
		*fileLength = fread(const_cast<uint8_t*>(*data), 1, *fileLength, file);
	}
}

void DartTestApp::writeFileCallback(const void* data, intptr_t length, void* file)
{
	fwrite(data, 1, length, reinterpret_cast<FILE*>(file));
}

void DartTestApp::closeFileCallback(void* file)
{
	fclose(reinterpret_cast<FILE*>(file));
}


void DartTestApp::update()
{
}

void DartTestApp::draw()
{
	gl::clear();
}

CINDER_APP_NATIVE( DartTestApp, RendererGl )
