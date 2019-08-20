#include <ork/kernel/msgrouter.inl>

namespace ork::msgrouter {

subscriber::subscriber(channel_impl* ch, msghandler_t h)
    : _channel(ch)
    , _handler(h){
}
subscriber::~subscriber(){
    _channel->unsubscribe(this);
}

subscriber_t channel_impl::subscribe(msghandler_t h){

    subscriber_t rv = std::make_shared<subscriber>(this,h);

    _subscribers.AtomicOp([=](subscriber_set_t& sst){
        sst.insert(rv.get());
    });

    return rv;

}
void channel_impl::unsubscribe(subscriber* s){
    _subscribers.AtomicOp([=](subscriber_set_t& sst){
        sst.erase(s);
    });
}
void channel_impl::post(content_t content){
    _subscribers.AtomicOp([&](subscriber_set_t& sst){
        for( auto item : sst ){
            item->_handler(content);
        }
    });

}

typedef std::unordered_map<std::string,channel_impl*> channelmap_t;
static LockedResource<channelmap_t> _channelmap;

channel_impl* channel(std::string key){

    channel_impl* rval = nullptr;

    _channelmap.AtomicOp([&](channelmap_t& cmap){
        auto it = cmap.find(key);
        if( it == cmap.end() ){
            rval = new channel_impl;
            cmap[key] = rval;
        }
        else
            rval = it->second;
    });

    return rval;
}

} // namespace ork::msgrouter {
