#pragma once

#include <functional>
#include <ork/kernel/svariant.h>
#include <ork/kernel/mutex.h>
#include <unordered_map>

namespace ork::msgrouter {

typedef svar64_t content_t;
typedef std::function<void(content_t)> msghandler_t;
typedef void* msgkey_t;
struct channel_impl;

struct subscriber {

    subscriber(channel_impl* ch, msghandler_t h);
    ~subscriber();
    msghandler_t _handler;
    channel_impl* _channel;
};

typedef std::shared_ptr<subscriber> subscriber_t;

struct channel_impl {

    typedef std::set<subscriber*>  subscriber_set_t;

    subscriber_t subscribe(msghandler_t h);
    void unsubscribe(subscriber* s);
    void post(content_t content);

    template <typename T, typename... A>
    inline void postType(A&&... args){
      content_t c;
      c.Make<T>(std::forward<A>(args)...);
      post(c);
    }

    LockedResource<subscriber_set_t> _subscribers;
};

channel_impl* channel(std::string key);

} // namespace ork::msgrouter
