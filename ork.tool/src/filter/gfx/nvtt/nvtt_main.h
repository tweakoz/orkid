////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//#define NVTT_SHARED 1
#define __SSE2__ 1
#define __SSE__ 1
#define __MMX__ 1
#undef small

///////////////////////////////////////////////////////////////////////////////

#include <nvtt/nvtt.h>
#include <nvcore/Debug.h>
#include <nvimage/Image.h>
#include <nvimage/ImageIO.h>
#include <nvimage/BlockDXT.h>
#include <nvimage/ColorBlock.h>
#include <nvcore/Ptr.h>
#include <nvcore/Debug.h>
#include <nvcore/StrLib.h>
#include <nvcore/StdStream.h>
#include <nvcore/TextWriter.h>
#include <nvcore/FileSystem.h>
#include <nvcore/Timer.h>

///////////////////////////////////////////////////////////////////////////////

struct MyMessageHandler : public nv::MessageHandler {
	MyMessageHandler() {
		nv::debug::setMessageHandler( this );
	}
	~MyMessageHandler() {
		nv::debug::resetMessageHandler();
	}

	virtual void log( const char * str, va_list arg ) {
		va_list val;
		va_copy(val, arg);
		vfprintf(stderr, str, arg);
		va_end(val);		
	}
};


struct MyAssertHandler : public nv::AssertHandler {
	MyAssertHandler() {
		nv::debug::setAssertHandler( this );
	}
	~MyAssertHandler() {
		nv::debug::resetAssertHandler();
	}
	
	// Handler method, note that func might be NULL!
	int assertion( const char *exp, const char *file, int line, const char *func, const char *msg, va_list arg ) final {
		fprintf(stderr, "Assertion failed: %s\nIn %s:%d\n", exp, file, line);
		nv::debug::dumpInfo();
		exit(1);
	}
};


struct MyOutputHandler : public nvtt::OutputHandler
{
	MyOutputHandler() : total(0), progress(0), percentage(0), stream(NULL) {}
	MyOutputHandler(const char * name) : total(0), progress(0), percentage(0), stream(new nv::StdOutputStream(name)) {}
	virtual ~MyOutputHandler() { delete stream; }
	
	bool open(const char * name)
	{
		stream = new nv::StdOutputStream(name);
		percentage = progress = 0;
		if (stream->isError()) {
			printf("Error opening '%s' for writting\n", name);
			return false;
		}
		return true;
	}
	
	virtual void setTotal(int t)
	{
		total = t;
	}

	virtual void mipmap(int size, int width, int height, int depth, int face, int miplevel)
	{
		// ignore.
	}
	
	// Output data.
	virtual bool writeData(const void * data, int size)
	{
		nvDebugCheck(stream != NULL);
		stream->serialize(const_cast<void *>(data), size);

		progress += size;
		int p = (100 * progress) / total;
		if (p != percentage)
		{
			percentage = p;
			printf("\r%d%%", percentage);
			fflush(stdout);
		}

		return true;
	}
	
	int total;
	int progress;
	int percentage;
	nv::StdOutputStream * stream;
};

struct MyErrorHandler : public nvtt::ErrorHandler
{
	virtual void error(nvtt::Error e)
	{
		nvDebugBreak();
	}
};

