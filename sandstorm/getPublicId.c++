// Sandstorm - Personal Cloud Sandbox
// Copyright (c) 2014 Sandstorm Development Group, Inc. and contributors
//

// Hack around stdlib bug with C++14.
#include <initializer_list> // force libstdc++ to include its config
#undef _GLIBCXX_HAVE_GETS // correct broken config
// End hack.

#include <kj/main.h>
#include <kj/debug.h>
#include <kj/async-io.h>
#include <kj/async-unix.h>
#include <kj/io.h>
#include <capnp/rpc-twoparty.h>
#include <capnp/rpc.capnp.h>
#include <capnp/ez-rpc.h>
#include <sandstorm/sandstorm-http-bridge.capnp.h>
#include <unistd.h>

#include <sandstorm/hack-session.capnp.h>

namespace sandstorm {

  class GetPublicIdMain {
  public:
    GetPublicIdMain(kj::ProcessContext& context): context(context) { }

    kj::MainFunc getMain() {
       return kj::MainBuilder(context, "GetPublicId version: 0.0.2",
                             "Runs the getPublicId command from hack-session.capnp. "
                             "Returns the ID and the host name as two lines on stdout.")
        .expectArg("<sessionId>", KJ_BIND_METHOD(*this, setSessionId))
        .callAfterParsing(KJ_BIND_METHOD(*this, run))
        .build();
    }

    kj::MainBuilder::Validity setSessionId(kj::StringPtr id) {
      sessionId = kj::heapString(id);
      return true;
    }

    kj::MainBuilder::Validity run() {
      capnp::EzRpcClient client("unix:/tmp/sandstorm-api");
      SandstormHttpBridge::Client restorer = client.getMain<SandstormHttpBridge>();

      auto request = restorer.getSessionContextRequest();
      request.setId(sessionId);
      auto session = request.send().getContext().castAs<HackSessionContext>();

      kj::Promise<void> promise = session.getPublicIdRequest().send().then(
        [](HackSessionContext::GetPublicIdResults::Reader result) {
          auto publicId = result.getPublicId();
          auto hostname = result.getHostname();
          auto autoUrl = result.getAutoUrl();
          kj::String msg =
            kj::str(
              "Your content has automatically been published at\n",
              "    ", autoUrl, "\n",
              "You can show this content on a custom domain by adding the following\n"
              "DNS records:\n"
              "                  <host> IN CNAME ", hostname, "\n"
              "    sandstorm-www.<host> IN TXT   ", publicId, "\n");
          kj::FdOutputStream(STDOUT_FILENO).write(msg.begin(), msg.size());
        }, [] (kj::Exception&& e) {
          auto desc = e.getDescription();
          kj::FdOutputStream(STDOUT_FILENO).write(desc.begin(), desc.size());
        });

      promise.wait(client.getWaitScope());
      return true;
    }

  private:
    kj::ProcessContext& context;
    kj::String sessionId;
  };

} // namespace sandstorm

KJ_MAIN(sandstorm::GetPublicIdMain)

