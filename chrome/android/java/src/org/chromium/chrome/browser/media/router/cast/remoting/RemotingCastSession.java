// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast.remoting;

import com.google.android.gms.cast.ApplicationMetadata;
import com.google.android.gms.cast.Cast;
import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.MediaStatus;
import com.google.android.gms.cast.RemoteMediaPlayer;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.media.router.cast.CastMessageHandler;
import org.chromium.chrome.browser.media.router.cast.CastSession;
import org.chromium.chrome.browser.media.router.cast.CastSessionInfo;
import org.chromium.chrome.browser.media.router.cast.CastSessionUtil;
import org.chromium.chrome.browser.media.router.cast.ChromeCastSessionManager;
import org.chromium.chrome.browser.media.router.cast.MediaSource;
import org.chromium.chrome.browser.media.ui.MediaNotificationInfo;
import org.chromium.chrome.browser.media.ui.MediaNotificationListener;
import org.chromium.chrome.browser.media.ui.MediaNotificationManager;

import java.util.HashSet;
import java.util.Set;

/**
 * A wrapper around a RemoteMediaPlayer, used in remote playback.
 */
public class RemotingCastSession implements MediaNotificationListener, CastSession {
    private final CastDevice mCastDevice;
    private final MediaSource mSource;

    private GoogleApiClient mApiClient;
    private String mSessionId;
    private String mApplicationStatus;
    private ApplicationMetadata mApplicationMetadata;
    private MediaNotificationInfo.Builder mNotificationBuilder;
    private RemoteMediaPlayer mMediaPlayer;
    private boolean mStoppingApplication = false;

    public RemotingCastSession(GoogleApiClient apiClient, String sessionId,
            ApplicationMetadata metadata, String applicationStatus, CastDevice castDevice,
            String origin, int tabId, boolean isIncognito, MediaSource source) {
        mCastDevice = castDevice;
        mSource = source;
        mApiClient = apiClient;
        mApplicationMetadata = metadata;
        mSessionId = sessionId;

        mMediaPlayer = new RemoteMediaPlayer();
        mMediaPlayer.setOnStatusUpdatedListener(new RemoteMediaPlayer.OnStatusUpdatedListener() {
            @Override
            public void onStatusUpdated() {
                MediaStatus mediaStatus = mMediaPlayer.getMediaStatus();
                if (mediaStatus == null) return;

                int playerState = mediaStatus.getPlayerState();
                if (playerState == MediaStatus.PLAYER_STATE_PAUSED
                        || playerState == MediaStatus.PLAYER_STATE_PLAYING) {
                    mNotificationBuilder.setPaused(playerState != MediaStatus.PLAYER_STATE_PLAYING);
                    mNotificationBuilder.setActions(MediaNotificationInfo.ACTION_STOP
                            | MediaNotificationInfo.ACTION_PLAY_PAUSE);
                } else {
                    mNotificationBuilder.setActions(MediaNotificationInfo.ACTION_STOP);
                }
                MediaNotificationManager.show(mNotificationBuilder.build());
            }
        });
        mMediaPlayer.setOnMetadataUpdatedListener(
                new RemoteMediaPlayer.OnMetadataUpdatedListener() {
                    @Override
                    public void onMetadataUpdated() {
                        CastSessionUtil.setNotificationMetadata(
                                mNotificationBuilder, mCastDevice, mMediaPlayer);
                        MediaNotificationManager.show(mNotificationBuilder.build());
                    }
                });

        mNotificationBuilder =
                new MediaNotificationInfo.Builder()
                        .setPaused(false)
                        .setOrigin(origin)
                        // TODO(avayvod): the same session might have more than one tab id. Should
                        // we track the last foreground alive tab and update the notification with
                        // it?
                        .setTabId(tabId)
                        .setPrivate(isIncognito)
                        .setActions(MediaNotificationInfo.ACTION_STOP)
                        .setNotificationSmallIcon(R.drawable.ic_notification_media_route)
                        .setDefaultNotificationLargeIcon(R.drawable.cast_playing_square)
                        .setId(R.id.presentation_notification)
                        .setListener(this);
        CastSessionUtil.setNotificationMetadata(mNotificationBuilder, mCastDevice, mMediaPlayer);
        MediaNotificationManager.show(mNotificationBuilder.build());
    }

    @Override
    public boolean isApiClientInvalid() {
        return mApiClient == null || !mApiClient.isConnected();
    }

    @Override
    public String getSourceId() {
        return mSource.getSourceId();
    }

    @Override
    public String getSinkId() {
        return mCastDevice.getDeviceId();
    }

    @Override
    public String getSessionId() {
        return mSessionId;
    }

    @Override
    public Set<String> getNamespaces() {
        return new HashSet<String>();
    }

    @Override
    public CastMessageHandler getMessageHandler() {
        return null;
    }

    @Override
    public CastSessionInfo getSessionInfo() {
        // Only used by the CastMessageHandler, which is unused in the RemotingCastSession case.
        return null;
    }

    @Override
    public boolean sendStringCastMessage(
            String message, String namespace, String clientId, int sequenceNumber) {
        // String messages are not used in remoting scenarios.
        return false;
    }

    @Override
    public HandleVolumeMessageResult handleVolumeMessage(
            JSONObject volume, String clientId, int sequenceNumber) throws JSONException {
        // RemoteMediaPlayer's setStreamVolume() should be used instead of volume messages.
        return null;
    }

    @Override
    public void stopApplication() {
        if (mStoppingApplication) return;

        if (isApiClientInvalid()) return;

        mStoppingApplication = true;
        Cast.CastApi.stopApplication(mApiClient, mSessionId)
                .setResultCallback(new ResultCallback<Status>() {
                    @Override
                    public void onResult(Status status) {
                        // TODO(https://crbug.com/535577): handle a failure to stop the application.

                        mSessionId = null;
                        mApiClient = null;

                        ChromeCastSessionManager.get().onSessionEnded();
                        mStoppingApplication = false;

                        MediaNotificationManager.clear(R.id.presentation_notification);
                    }
                });
    }

    @Override
    public void onClientConnected(String clientId) {}

    @Override
    public void onMediaMessage(String message) {
        if (mMediaPlayer != null)
            mMediaPlayer.onMessageReceived(mCastDevice, CastSessionUtil.MEDIA_NAMESPACE, message);
    }

    @Override
    public void onVolumeChanged() {}

    @Override
    public void updateSessionStatus() {}

    /////////////////////////////////////////////////////////////////////////////////////////////
    // MediaNotificationListener implementation.

    @Override
    public void onPlay(int actionSource) {
        if (mMediaPlayer == null || isApiClientInvalid()) return;

        mMediaPlayer.play(mApiClient);
    }

    @Override
    public void onPause(int actionSource) {
        if (mMediaPlayer == null || isApiClientInvalid()) return;

        mMediaPlayer.pause(mApiClient);
    }

    @Override
    public void onStop(int actionSource) {
        stopApplication();
        ChromeCastSessionManager.get().onSessionStopAction();
    }

    @Override
    public void onMediaSessionAction(int action) {}
}
