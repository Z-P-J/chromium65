// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/router/issue_manager.h"
#include "chrome/browser/media/router/media_router_base.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/common/media_router/route_request_result.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace content {
class BrowserContext;
}

namespace media_router {

enum class MediaRouteProviderWakeReason;

// MediaRouter implementation that delegates calls to a MediaRouteProvider.
class MediaRouterMojoImpl : public MediaRouterBase,
                            public mojom::MediaRouter {
 public:
  ~MediaRouterMojoImpl() override;

  // MediaRouter implementation.
  void CreateRoute(const MediaSource::Id& source_id,
                   const MediaSink::Id& sink_id,
                   const url::Origin& origin,
                   content::WebContents* web_contents,
                   std::vector<MediaRouteResponseCallback> callbacks,
                   base::TimeDelta timeout,
                   bool incognito) final;
  void JoinRoute(const MediaSource::Id& source_id,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 content::WebContents* web_contents,
                 std::vector<MediaRouteResponseCallback> callbacks,
                 base::TimeDelta timeout,
                 bool incognito) final;
  void ConnectRouteByRouteId(const MediaSource::Id& source,
                             const MediaRoute::Id& route_id,
                             const url::Origin& origin,
                             content::WebContents* web_contents,
                             std::vector<MediaRouteResponseCallback> callbacks,
                             base::TimeDelta timeout,
                             bool incognito) final;
  void TerminateRoute(const MediaRoute::Id& route_id) final;
  void DetachRoute(const MediaRoute::Id& route_id) final;
  void SendRouteMessage(const MediaRoute::Id& route_id,
                        const std::string& message,
                        SendRouteMessageCallback callback) final;
  void SendRouteBinaryMessage(const MediaRoute::Id& route_id,
                              std::unique_ptr<std::vector<uint8_t>> data,
                              SendRouteMessageCallback callback) final;
  void OnUserGesture() override;
  void SearchSinks(const MediaSink::Id& sink_id,
                   const MediaSource::Id& source_id,
                   const std::string& search_input,
                   const std::string& domain,
                   MediaSinkSearchResponseCallback sink_callback) final;
  scoped_refptr<MediaRouteController> GetRouteController(
      const MediaRoute::Id& route_id) final;
  void RegisterMediaRouteProvider(
      MediaRouteProviderId provider_id,
      mojom::MediaRouteProviderPtr media_route_provider_ptr,
      mojom::MediaRouter::RegisterMediaRouteProviderCallback callback) override;

  // Issues 0+ calls to the provider given by |provider_id| to ensure its state
  // is in sync with MediaRouter on a best-effort basis.
  virtual void SyncStateToMediaRouteProvider(MediaRouteProviderId provider_id);

  const std::string& instance_id() const { return instance_id_; }

  void set_instance_id_for_test(const std::string& instance_id) {
    instance_id_ = instance_id;
  }

 protected:
  // Standard constructor, used by
  // MediaRouterMojoImplFactory::GetApiForBrowserContext.
  explicit MediaRouterMojoImpl(content::BrowserContext* context);

  // Requests MRPs to update media sinks.  This allows MRPs that only do
  // discovery on sink queries an opportunity to update discovery results
  // even if the MRP SinkAvailability is marked UNAVAILABLE.
  void UpdateMediaSinks(const MediaSource::Id& source_id);

  // Requests the creation of a MediaRouteController implementation by passing
  // the interface request to |media_route_provider_|. Does not take ownership
  // of |route_controller|.
  void InitMediaRouteController(MediaRouteController* route_controller);

  // Called when the Mojo pointer for |provider_id| has a connection error.
  // Removes the pointer from |media_route_providers_|.
  void OnProviderConnectionError(MediaRouteProviderId provider_id);

  // Creates a binding between |this| and |request|.
  void BindToMojoRequest(mojo::InterfaceRequest<mojom::MediaRouter> request);

  // Returns the ID of the provider associated with the presentation ID, or
  // nullopt if not found.
  virtual base::Optional<MediaRouteProviderId> GetProviderIdForPresentation(
      const std::string& presentation_id);

  content::BrowserContext* context() const { return context_; }

  // Mojo pointers to media route providers. Providers are added via
  // RegisterMediaRouteProvider().
  base::flat_map<MediaRouteProviderId, mojom::MediaRouteProviderPtr>
      media_route_providers_;

  // Stores route controllers that can be used to send media commands.
  base::flat_map<MediaRoute::Id, MediaRouteController*> route_controllers_;

 private:
  friend class MediaRouterFactory;
  friend class MediaRouterMojoImplTest;
  friend class MediaRouterMojoTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, JoinRoute);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, JoinRouteTimedOutFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           JoinRouteIncognitoMismatchFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           IncognitoRoutesTerminatedOnProfileShutdown);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterAndUnregisterMediaSinksObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterMediaSinksObserverWithAvailabilityChange);
  FRIEND_TEST_ALL_PREFIXES(
      MediaRouterMojoImplTest,
      RegisterAndUnregisterMediaSinksObserverWithAvailabilityChange);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterAndUnregisterMediaRoutesObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RouteMessagesSingleObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RouteMessagesMultipleObservers);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, HandleIssue);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, GetRouteController);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           GetRouteControllerMultipleTimes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           GetRouteControllerAfterInvalidation);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           GetRouteControllerAfterRouteInvalidation);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           FailToCreateRouteController);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           RegisterMediaRoutesObserver_DedupingWithCache);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallback);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallbackRemoved);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           SendSinkRequestsToMultipleProviders);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           SendRouteRequestsToMultipleProviders);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           ObserveSinksFromMultipleProviders);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           ObserveRoutesFromMultipleProviders);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           SyncStateToMediaRouteProvider);
  FRIEND_TEST_ALL_PREFIXES(ExtensionMediaRouteProviderProxyTest,
                           StartAndStopObservingMediaSinks);

  // Represents a query to the MediaRouteProviders for media sinks and caches
  // media sinks returned by MRPs. Holds observers for the query.
  class MediaSinksQuery {
   public:
    MediaSinksQuery();
    ~MediaSinksQuery();

    // Caches the list of sinks for the provider returned from the query.
    void SetSinksForProvider(MediaRouteProviderId provider_id,
                             const std::vector<MediaSink>& sinks);

    // Resets the internal state, including the cache for all the providers.
    void Reset();

    void AddObserver(MediaSinksObserver* observer);
    void RemoveObserver(MediaSinksObserver* observer);
    void NotifyObservers();
    bool HasObserver(MediaSinksObserver* observer) const;
    bool HasObservers() const;

    const std::vector<MediaSink>& cached_sink_list() const {
      return cached_sink_list_;
    }
    void set_origins(const std::vector<url::Origin>& origins) {
      origins_ = origins;
    }

   private:
    // Cached list of sinks for the query.
    std::vector<MediaSink> cached_sink_list_;

    // Cached list of origins for the query.
    // TODO(takumif): The list of supported origins may differ between MRPs, so
    // we need more fine-grained associations between sinks and origins.
    std::vector<url::Origin> origins_;

    base::ObserverList<MediaSinksObserver> observers_;

    DISALLOW_COPY_AND_ASSIGN(MediaSinksQuery);
  };

  // Represents a query to the MediaRouteProviders for media routes and caches
  // media routes returned by MRPs. Holds observers for the query.
  class MediaRoutesQuery {
   public:
    MediaRoutesQuery();
    ~MediaRoutesQuery();

    // Caches the list of routes and joinable route IDs for the provider
    // returned from the query.
    void SetRoutesForProvider(
        MediaRouteProviderId provider_id,
        const std::vector<MediaRoute>& routes,
        const std::vector<MediaRoute::Id>& joinable_route_ids);

    // Adds |route| to the list of routes managed by the provider and returns
    // true, if it hasn't been added already. Returns false otherwise.
    bool AddRouteForProvider(MediaRouteProviderId provider_id,
                             const MediaRoute& route);

    // Re-constructs |cached_route_list_| by merging route lists in
    // |providers_to_routes_|.
    void UpdateCachedRouteList();

    void AddObserver(MediaRoutesObserver* observer);
    void RemoveObserver(MediaRoutesObserver* observer);
    void NotifyObservers();
    bool HasObserver(MediaRoutesObserver* observer) const;
    bool HasObservers() const;

    const base::Optional<std::vector<MediaRoute>>& cached_route_list() const {
      return cached_route_list_;
    }
    const std::vector<MediaRoute::Id>& joinable_route_ids() const {
      return joinable_route_ids_;
    }
    const base::flat_map<MediaRouteProviderId, std::vector<MediaRoute>>&
    providers_to_routes() const {
      return providers_to_routes_;
    }

   private:
    // Cached list of routes and joinable route IDs for the query.
    base::Optional<std::vector<MediaRoute>> cached_route_list_;
    std::vector<MediaRoute::Id> joinable_route_ids_;

    // Per-MRP lists of routes and joinable route IDs for the query.
    // TODO(crbug.com/761493): Consider making MRP ID an attribute
    // of MediaRoute, so that we can simplify these into vectors.
    base::flat_map<MediaRouteProviderId, std::vector<MediaRoute>>
        providers_to_routes_;
    base::flat_map<MediaRouteProviderId, std::vector<MediaRoute::Id>>
        providers_to_joinable_routes_;

    base::ObserverList<MediaRoutesObserver> observers_;

    DISALLOW_COPY_AND_ASSIGN(MediaRoutesQuery);
  };

  class ProviderSinkAvailability {
   public:
    ProviderSinkAvailability();
    ~ProviderSinkAvailability();

    // Sets the sink availability for |provider_id|. Returns true if
    // |availability| is different from that already recorded.
    bool SetAvailabilityForProvider(MediaRouteProviderId provider_id,
                                    SinkAvailability availability);

    // Returns true if the availability for the provider is not UNAVAILABLE.
    bool IsAvailableForProvider(MediaRouteProviderId provider_id) const;

    // Returns true if there is a provider whose sink availability isn't
    // UNAVAILABLE.
    bool IsAvailable() const;

   private:
    void UpdateOverallAvailability();

    base::flat_map<MediaRouteProviderId, SinkAvailability> availabilities_;
    SinkAvailability overall_availability_ = SinkAvailability::UNAVAILABLE;
  };

  // MediaRouter implementation.
  bool RegisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void UnregisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void RegisterRouteMessageObserver(RouteMessageObserver* observer) override;
  void UnregisterRouteMessageObserver(RouteMessageObserver* observer) override;
  void DetachRouteController(const MediaRoute::Id& route_id,
                             MediaRouteController* controller) override;

  // Notifies |observer| of any existing cached routes, if it is still
  // registered.
  void NotifyOfExistingRoutesIfRegistered(const MediaSource::Id& source_id,
                                          MediaRoutesObserver* observer) const;

  // mojom::MediaRouter implementation.
  void OnIssue(const IssueInfo& issue) override;
  void OnSinksReceived(MediaRouteProviderId provider_id,
                       const std::string& media_source,
                       const std::vector<MediaSinkInternal>& internal_sinks,
                       const std::vector<url::Origin>& origins) override;
  void OnRoutesUpdated(
      MediaRouteProviderId provider_id,
      const std::vector<MediaRoute>& routes,
      const std::string& media_source,
      const std::vector<std::string>& joinable_route_ids) override;
  void OnSinkAvailabilityUpdated(MediaRouteProviderId provider_id,
                                 SinkAvailability availability) override;
  void OnPresentationConnectionStateChanged(
      const std::string& route_id,
      content::PresentationConnectionState state) override;
  void OnPresentationConnectionClosed(
      const std::string& route_id,
      content::PresentationConnectionCloseReason reason,
      const std::string& message) override;
  void OnRouteMessagesReceived(
      const std::string& route_id,
      const std::vector<content::PresentationConnectionMessage>& messages)
      override;
  void OnMediaRemoterCreated(
      int32_t tab_id,
      media::mojom::MirrorServiceRemoterPtr remoter,
      media::mojom::MirrorServiceRemotingSourceRequest source_request) override;

  // Result callback when Mojo TerminateRoute is invoked.
  // |route_id|: ID of MediaRoute passed to the TerminateRoute request.
  // |provider_id|: ID of MediaRouteProvider that handled the request.
  // |error_text|: Error message if an error occurred.
  // |result_code|: The result of the request.
  void OnTerminateRouteResult(const MediaRoute::Id& route_id,
                              MediaRouteProviderId provider_id,
                              const base::Optional<std::string>& error_text,
                              RouteRequestResult::ResultCode result_code);

  // Adds |route| to the list of routes. Called in the callback for
  // CreateRoute() etc. so that even if the callback is called before
  // OnRoutesUpdated(), MediaRouter is still aware of the route.
  void OnRouteAdded(MediaRouteProviderId provider_id, const MediaRoute& route);

  // Converts the callback result of calling Mojo CreateRoute()/JoinRoute()
  // into a local callback.
  void RouteResponseReceived(const std::string& presentation_id,
                             MediaRouteProviderId provider_id,
                             bool is_incognito,
                             std::vector<MediaRouteResponseCallback> callbacks,
                             bool is_join,
                             const base::Optional<MediaRoute>& media_route,
                             const base::Optional<std::string>& error_text,
                             RouteRequestResult::ResultCode result_code);

  // Callback called by MRP's CreateMediaRouteController().
  void OnMediaControllerCreated(const MediaRoute::Id& route_id, bool success);

  // Invalidates and removes controllers from |route_controllers_| whose media
  // routes do not appear in |routes|.
  void RemoveInvalidRouteControllers(const std::vector<MediaRoute>& routes);

  // Methods for obtaining a pointer to the provider associated with the given
  // object. They return a nullopt when such a provider is not found.
  base::Optional<MediaRouteProviderId> GetProviderIdForRoute(
      const MediaRoute::Id& route_id);
  base::Optional<MediaRouteProviderId> GetProviderIdForSink(
      const MediaSink::Id& sink_id);

  // Gets the sink with the given ID from lists of sinks held by sink queries.
  // Returns a nullptr if none is found.
  const MediaSink* GetSinkById(const MediaSink::Id& sink_id) const;

  base::flat_map<MediaSource::Id, std::unique_ptr<MediaSinksQuery>>
      sinks_queries_;

  base::flat_map<MediaSource::Id, std::unique_ptr<MediaRoutesQuery>>
      routes_queries_;

  base::flat_map<MediaRoute::Id,
                 std::unique_ptr<base::ObserverList<RouteMessageObserver>>>
      message_observers_;

  // GUID unique to each browser run. Component extension uses this to detect
  // when its persisted state was written by an older browser instance, and is
  // therefore stale.
  std::string instance_id_;

  // The last reported sink availability from the media route providers.
  ProviderSinkAvailability sink_availability_;

  // Bindings for Mojo pointers to |this| held by media route providers.
  mojo::BindingSet<mojom::MediaRouter> bindings_;

  content::BrowserContext* const context_;

  base::WeakPtrFactory<MediaRouterMojoImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterMojoImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_
