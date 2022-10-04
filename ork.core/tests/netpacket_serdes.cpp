#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <memory>
#include <ork/kernel/netpacket_serdes.inl>

struct CustomSerdesObject {

	float _a = 2;
	float _b = 3;
	int _x = 0;
};

using custom_serdes_object_ptr_t = std::shared_ptr<CustomSerdesObject>;

TEST(NetPacketSerdes) {

	using namespace ork;
	using namespace ork::net::serdes;

  printf("//////////////////////////////////////\n");
  printf("ORK NetPacketSerdes TEST\n");
  printf("//////////////////////////////////////\n");

  ////////////////////////////////////////////
  // construct kvmap 
  ////////////////////////////////////////////

  kvmap_t kvmap;

  // make an int32_vector_t out of kvmap["i32v"]
	auto& i32v1 = kvmap["i32v"].make<int32vector_t>();
	i32v1.push_back(3);
	i32v1.push_back(4);
	i32v1.push_back(5);

  // make an int64vector_t out of kvmap["i64v"]
	auto& i64v1 = kvmap["i64v"].make<int64vector_t>();
	i64v1.push_back(3);
	i64v1.push_back(4);
	i64v1.push_back(5);

  // make a float_vect_t out of kvmap["fv"]
	auto& fv1 = kvmap["fv"].make<float_vect_t>();
	fv1.push_back(3);
	fv1.push_back(4);
	fv1.push_back(5);

  // make a double_vect_t out of kvmap["dv"]
	auto& dv1 = kvmap["dv"].make<double_vect_t>();
	dv1.push_back(3);
	dv1.push_back(4);
	dv1.push_back(5);

  ////////////////////////////////////////////

	auto& custom = kvmap["custom"].makeShared<CustomSerdesObject>();
	custom->_a = 7.0f;
	custom->_b = 8.0f;
	custom->_x = 9;

  ////////////////////////////////////////////
	// custom serializer/deserializer
  ////////////////////////////////////////////

	auto serializer = std::make_shared<Serializer>();
	auto deserializer = std::make_shared<Deserializer>();

  serializer->registerType<custom_serdes_object_ptr_t>([](Serializer* ser, //
  	                                                      msgpacketbase_ref_t msg, //
  	                                                      const val_t& inp_value) {
    msg.writeString("my_custom_type");
    const auto& typed_inp = inp_value.get<custom_serdes_object_ptr_t>();
    msg.write<float>(typed_inp->_a);
    msg.write<float>(typed_inp->_b);
    msg.write<int>(typed_inp->_x);
  });

  deserializer->registerType("my_custom_type", [](Deserializer* deser, //
  	                                              MessagePacketIteratorBase& iter, //
  	                                              val_t& out_value,
  	                                              const on_fixup_t& on_fixup) {

  	const auto& packet = iter._basepacket;
    auto typed_out = out_value.makeShared<CustomSerdesObject>();
    packet.read<float>(typed_out->_a,iter);
    packet.read<float>(typed_out->_b,iter);
    packet.read<int>(typed_out->_x,iter);
  });

  ////////////////////////////////////////////
  // serialize kvmap to packet
  ////////////////////////////////////////////

	auto pkt = std::make_shared<DynamicMessagePacket>();

	serializer->serialize(*pkt,kvmap);

  ////////////////////////////////////////////
  // deserialize packet into out_value
  ////////////////////////////////////////////

	auto iter = pkt->makeIterator();
	net::serdes::val_t out_value;
	deserializer->deserialize(iter,out_value);

  ////////////////////////////////////////////
	// we know out_value is supposed to be a kvmap_t
  ////////////////////////////////////////////

	const auto& test_as_kv = out_value.get<kvmap_t>();

  ////////////////////////////////////////////
	// check data in kvmap
  ////////////////////////////////////////////

	const auto& it_test_fv1 = test_as_kv.find("fv");
	const auto& it_test_dv1 = test_as_kv.find("dv");
	const auto& it_test_i32v1 = test_as_kv.find("i32v");
	const auto& it_test_i64v1 = test_as_kv.find("i64v");
	const auto& it_test_custom = test_as_kv.find("custom");

	const auto& test_fv1 = it_test_fv1->second.get<float_vect_t>();
	const auto& test_dv1 = it_test_dv1->second.get<double_vect_t>();
	const auto& test_i32v1 = it_test_i32v1->second.get<int32vector_t>();
	const auto& test_i64v1 = it_test_i64v1->second.get<int64vector_t>();
	const auto& test_custom = it_test_custom->second.get<custom_serdes_object_ptr_t>();

	CHECK_EQUAL(test_fv1.size(), 3 );
	CHECK_EQUAL(test_dv1.size(), 3 );
	CHECK_EQUAL(test_i32v1.size(), 3 );
	CHECK_EQUAL(test_i64v1.size(), 3 );

	CHECK_EQUAL(test_fv1[0], 3.0f );
	CHECK_EQUAL(test_fv1[1], 4.0f );
	CHECK_EQUAL(test_fv1[2], 5.0f );

	CHECK_EQUAL(test_i32v1[0], 3 );
	CHECK_EQUAL(test_i32v1[1], 4 );
	CHECK_EQUAL(test_i32v1[2], 5 );

	CHECK_EQUAL(test_i64v1[0], 3 );
	CHECK_EQUAL(test_i64v1[1], 4 );
	CHECK_EQUAL(test_i64v1[2], 5 );

	CHECK_EQUAL(test_custom->_a, 7.0f );
	CHECK_EQUAL(test_custom->_b, 8.0f );
	CHECK_EQUAL(test_custom->_x, 9 );

  ////////////////////////////////////////////
}