////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/ecs/scene.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/system.h>
#include <ork/ecs/datatable.h>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>

#include "message_private.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

using namespace ::ork;

Controller::TraceWriter::TraceWriter(Controller* c, file::Path path) : _controller(c) {

	auto abspath = path.toAbsolute();
	printf( "abspath<%s>\n", abspath.c_str() );
	_output_file = fopen(abspath.c_str(),"wt");
	fprintf(_output_file,"[\n");
	fflush(_output_file);

	_outtimer.Start();
}

///////////////////////////////////////////////////////////////////////////////

Controller::TraceWriter::~TraceWriter(){
	fprintf(_output_file,"\n]\n");
	fflush(_output_file);
	fclose(_output_file);
}

///////////////////////////////////////////////////////////////////////////////

std::string Controller::TraceWriter::_traceTable(const DataTable& table){

	std::string rval = "[";

	bool first= true;
	for( auto item : table._items ){

		const auto& K = item._key;
		const auto& V = item._val;

		auto KS = _traceVar64(K._encoded);
		auto VS = _traceVar64(V._encoded);

		if(not first)
			rval += ",";

		rval += "{";
		rval += FormatString("\"key\": %s ", KS.c_str() );
		rval += FormatString(", \"val\": %s ", VS.c_str() );
		rval += "}";

		first = false;
	}

	rval += "]";
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string Controller::TraceWriter::_traceVar64(const svar64_t& var){

	std::string rval;
	/////////////////////////////////////////////////////////////
	if( auto as_FLT = var.tryAs<float>() ){
		rval = FormatString("{\"float\": %g}", as_FLT.value() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_DBL = var.tryAs<double>() ){
		rval = FormatString("{\"double\": %g}", as_FLT.value() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_int = var.tryAs<int>() ){
		rval = FormatString("{\"int\": %d}", as_int.value() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_uint64_t = var.tryAs<uint64_t>() ){
		rval = FormatString("{\"uint64_t\": %zu}", as_uint64_t.value() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_fv3 = var.tryAs<fvec3>() ){
		rval = FormatString("{\"fvec3\": [%g,%g,%g]}", //
			as_fv3.value().x, as_fv3.value().y, as_fv3.value().z );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_fq = var.tryAs<fquat>() ){
		rval = FormatString("{\"fquat\": [%g,%g,%g,%g]}", //
			as_fq.value().x, as_fq.value().y, as_fq.value().z, as_fq.value().w );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_STR = var.tryAs<std::string>() ){
		rval = FormatString("{\"stdstr\": %g}", as_STR.value().c_str() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_MDATA = var.tryAs<lev2::modeldrawabledata_ptr_t>() ){

		/////////////////////////////////////////////
		// TODO - reference modeldata from SCENE!
		/////////////////////////////////////////////

		rval = FormatString("{\"modeldata\": { ");
		rval += FormatString("\"path\": \"%s\"", as_MDATA.value()->_assetpath.c_str() );
		rval += "}}";

	}
	/////////////////////////////////////////////////////////////
	else if( auto as_TOK = var.tryAs<token_t>() ){

		auto tokstr = detokenize(as_TOK.value());

		if(tokstr.length()){
			rval = FormatString("{\"token\": \"%s\" }", tokstr.c_str());
		}
		else{
			rval = FormatString("{\"token\": %zu }", as_TOK.value().hashed());
		}

	}
	/////////////////////////////////////////////////////////////
	else if( auto as_RESPREF = var.tryAs<ResponseRef>() ){
		rval = FormatString("{\"respref\": { ");
		rval += FormatString("\"respID\": %zu", as_RESPREF.value()._responseID);
		rval += "}}";
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_TABLE = var.tryAs<DataTable>() ){
		auto tabstr = _traceTable(as_TABLE.value());
		rval = FormatString("{\"table\":  %s }" , tabstr.c_str() );
	}
	else{
		printf( "unknown var type<%s>\n", var.typeName() );
		OrkAssert(false);
	}
	return rval;
};

///////////////////////////////////////////////////////////////////////////////

std::string Controller::TraceWriter::_traceVar128(const svar128_t& var){

	std::string rval;

	/////////////////////////////////////////////////////////////
	if( auto as_SND = var.tryAs<impl::_SpawnNamedDynamic>() ){

		std::string payload_snd;
		std::string payload_srec;
		std::string payload_entref;

		rval = FormatString("\"SpawnNamedDynamic\":{\"SND\": {%s} , \"SREC\": {%s} , \"ENTREF\": {%s}  }", //
			                  payload_snd.c_str(), //
			                  payload_srec.c_str(), //
			                  payload_entref.c_str() //
			                  );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_X = var.tryAs<impl::_SpawnAnonDynamic>() ){

		const auto& value = as_X.value();

		std::string payload_srec;

		if( value._spawn_rec ){

			auto XF = value._spawn_rec->transform();

			auto xfs = FormatString("{ \"pos\": [%g,%g,%g] , \"rot\": [%g,%g,%g,%g], \"scale\": %g }", //
															XF->_translation.x, XF->_translation.y, XF->_translation.z, //
															XF->_rotation.x, XF->_rotation.y, XF->_rotation.z, XF->_rotation.w, //
															XF->_uniformScale ); //

			auto arch = value._spawn_rec->GetArchetype();
			auto archname = FormatString("\"%s\"", arch->GetName().c_str() );
			payload_srec = FormatString("{ \"name\": \"%s\", \"arch\": %s , \"xf\": %s }",
				value._spawn_rec->GetName().c_str(),
				archname.c_str(),
				xfs.c_str()
			);

		} else {
			payload_srec = "\"none\"";
		}

		rval = FormatString("\"SpawnAnonDynamic\":{\"edata\": \"%s\", \"entID\": %zu, \"SREC\": %s  }", //
			                  value._SAD._edataname.c_str(), //
			                  value._entref._entID, //
			                  payload_srec.c_str() //
			                  );

	}
	/////////////////////////////////////////////////////////////
	else if( auto as_X = var.tryAs<impl::_Despawn>() ){
		const auto& value = as_X.value();
		auto payload_fs = FormatString(" \"entID\": %zu",	 value._entref._entID);
		rval = FormatString("\"Despawn\":{ %s }", payload_fs.c_str() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_X = var.tryAs<impl::_FindSystem>() ){

		const auto& value = as_X.value();
		auto syskey = std::string(value._syskey);

		auto payload_fs = FormatString(" \"sysID\": %zu, ",value._sysref._sysID );
		payload_fs += FormatString("\"syskey\": \"%s\"",syskey.c_str());
		rval = FormatString("\"FindSystem\":{ %s }", payload_fs.c_str() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_X = var.tryAs<impl::_FindComponent>() ){
		const auto& value = as_X.value();
		auto payload_fs = FormatString(" \"entID\": %zu",	 value._entref._entID);
		payload_fs += FormatString(", \"compID\": %zu",value._compref._compID);
		payload_fs += FormatString(", \"class\": \"%s\"",value._compclazz->Name().c_str());
		rval = FormatString("\"FindComponent\":{ %s }", payload_fs.c_str() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_X = var.tryAs<impl::_SystemEvent>() ){
		const auto& value = as_X.value();
		auto evnstr = _traceVar64(value._eventID);

		auto evndatastr = _traceVar64(value._eventData);

		auto payload_fs = FormatString(" \"sysID\": %zu",value._sysref._sysID );
		payload_fs += FormatString(", \"evnID\": %s",evnstr.c_str());
		payload_fs += FormatString(", \"evnDATA\": %s",evndatastr.c_str());

		rval = FormatString("\"SystemEvent\":{ %s }", payload_fs.c_str() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_X = var.tryAs<impl::_ComponentEvent>() ){
		const auto& value = as_X.value();
		auto evnstr = _traceVar64(value._eventID);

		auto evndatastr = _traceVar64(value._eventData);

		auto payload_fs = FormatString(" \"compID\": %zu",value._compref._compID );
		payload_fs += FormatString(", \"evnID\": %s",evnstr.c_str());
		payload_fs += FormatString(", \"evnDATA\": %s",evndatastr.c_str());

		rval = FormatString("\"ComponentEvent\":{ %s }", payload_fs.c_str() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_X = var.tryAs<impl::_SystemRequest>() ){

		const auto& value = as_X.value();
		auto reqstr = _traceVar64(value._requestID);

		auto reqdatastr = _traceVar64(value._eventData);

		auto payload_fs = FormatString(" \"sysID\": %zu",value._sysref._sysID );
		payload_fs += FormatString(", \"reqID\": %s",reqstr.c_str());
		payload_fs += FormatString(", \"responseID\": %zu",value._respref._responseID);
		payload_fs += FormatString(", \"reqDATA\": %s",reqdatastr.c_str());

		rval = FormatString("\"SystemRequest\":{ %s }", payload_fs.c_str() );
	}
	/////////////////////////////////////////////////////////////
	else if( auto as_X = var.tryAs<impl::_ComponentRequest>() ){
		const auto& value = as_X.value();
		auto reqstr = _traceVar64(value._requestID);

		auto reqdatastr = _traceVar64(value._eventData);

		auto payload_fs = FormatString(" \"compID\": %zu",value._compref._compID );
		payload_fs += FormatString(", \"reqID\": %s",reqstr.c_str());
		payload_fs += FormatString(", \"responseID\": %zu",value._respref._responseID);
		payload_fs += FormatString(", \"reqDATA\": %s",reqdatastr.c_str());

		rval = FormatString("\"ComponentRequest\":{ %s }", payload_fs.c_str() );
	}
	/////////////////////////////////////////////////////////////

	else{
		OrkAssert(false);
	}

	return rval;

}

///////////////////////////////////////////////////////////////////////////////

void Controller::TraceWriter::_traceEvent(const Event& event){

	float timestamp_offset = _outtimer.SecsSinceStart();

	std::string payload_output = _traceVar128(event._payload);
	std::string prefix = _firstitem ? "\n" : ",\n";
	std::string output = FormatString("%s{\"timestamp\": %g, %s }", //
		prefix.c_str(), //
		 //event._eventID,  //
		 timestamp_offset,  //
		 payload_output.c_str() );

	fprintf(_output_file,"%s", output.c_str());
	fflush(_output_file);

	_firstitem = false;
}

///////////////////////////////////////////////////////////////////////////////

void Controller::TraceWriter::_traceRequest(const Request& request){

	float timestamp_offset = _outtimer.SecsSinceStart();

	std::string payload_output = _traceVar128(request._payload);

	std::string prefix = _firstitem ? "\n" : ",\n";

	std::string output = FormatString("%s{\"timestamp\": %g, %s }", //
		prefix.c_str(), //
		 //request._requestID,  //
		 timestamp_offset,  //
		 payload_output.c_str() );
	fprintf(_output_file,"%s", output.c_str());
	fflush(_output_file);

	_firstitem = false;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
