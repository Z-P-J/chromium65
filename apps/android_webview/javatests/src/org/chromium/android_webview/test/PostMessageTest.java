// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.webkit.JavascriptInterface;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.browser.AppWebMessagePort;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.CountDownLatch;

/**
 * The tests for content postMessage API.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class PostMessageTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final String SOURCE_ORIGIN = "";
    // Timeout to failure, in milliseconds
    private static final long TIMEOUT = scaleTimeout(5000);

    // Inject to the page to verify received messages.
    private static class MessageObject {
        private boolean mReady;
        private String mData;
        private String mOrigin;
        private int[] mPorts;
        private final Object mLock = new Object();

        @JavascriptInterface
        public void setMessageParams(String data, String origin, int[] ports) {
            synchronized (mLock) {
                mData = data;
                mOrigin = origin;
                mPorts = ports;
                mReady = true;
                mLock.notify();
            }
        }

        public void waitForMessage() throws InterruptedException {
            synchronized (mLock) {
                long deadline = System.currentTimeMillis() + TIMEOUT;
                while (!mReady && System.currentTimeMillis() < deadline) {
                    mLock.wait(deadline - System.currentTimeMillis());
                }
            }
        }

        public String getData() {
            return mData;
        }

        public String getOrigin() {
            return mOrigin;
        }

        public int[] getPorts() {
            return mPorts;
        }
    }

    private MessageObject mMessageObject;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mMessageObject = new MessageObject();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        try {
            InstrumentationRegistry.getInstrumentation().runOnMainSync(
                    () -> mAwContents.addJavascriptInterface(mMessageObject, "messageObject"));
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Exception {
        mWebServer.shutdown();
    }

    private static final String WEBVIEW_MESSAGE = "from_webview";
    private static final String JS_MESSAGE = "from_js";

    private static final String TEST_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        onmessage = function (e) {"
            + "            messageObject.setMessageParams(e.data, e.origin, e.ports);"
            + "            if (e.ports != null && e.ports.length > 0) {"
            + "               e.ports[0].postMessage(\"" + JS_MESSAGE + "\");"
            + "            }"
            + "        }"
            + "   </script>"
            + "</body></html>";

    // Concats all the data fields of the received messages and makes it
    // available as page title.
    private static final String TITLE_FROM_POSTMESSAGE_TO_FRAME =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        var received = '';"
            + "        onmessage = function (e) {"
            + "            received += e.data;"
            + "            document.title = received;"
            + "        }"
            + "   </script>"
            + "</body></html>";

    // Concats all the data fields of the received messages to the transferred channel
    // and makes it available as page title.
    private static final String TITLE_FROM_POSTMESSAGE_TO_CHANNEL =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        var received = '';"
            + "        onmessage = function (e) {"
            + "            var myport = e.ports[0];"
            + "            myport.onmessage = function (f) {"
            + "                received += f.data;"
            + "                document.title = received;"
            + "            }"
            + "        }"
            + "   </script>"
            + "</body></html>";

    // Call on non-UI thread.
    private void expectTitle(String title) throws Throwable {
        CriteriaHelper.pollUiThread(Criteria.equals(title, () -> mAwContents.getTitle()));
    }

    private void loadPage(String page) throws Throwable {
        final String url = mWebServer.setResponse("/test.html", page,
                CommonResources.getTextHtmlHeaders(true));
        OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        onPageFinishedHelper.waitForCallback(currentCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrame() throws Throwable {
        verifyPostMessageToMainFrame(mWebServer.getBaseUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrameUsingWildcard() throws Throwable {
        verifyPostMessageToMainFrame("*");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrameUsingEmptyStringAsWildcard() throws Throwable {
        verifyPostMessageToMainFrame("");
    }

    private void verifyPostMessageToMainFrame(final String targetOrigin) throws Throwable {
        loadPage(TEST_PAGE);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, targetOrigin, null));
        mMessageObject.waitForMessage();
        Assert.assertEquals(WEBVIEW_MESSAGE, mMessageObject.getData());
        Assert.assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferringSamePortTwiceViaPostMessageToFrameNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            mAwContents.postMessageToFrame(
                    null, "1", mWebServer.getBaseUrl(), new AppWebMessagePort[]{channel[1]});
            // Retransfer the port. This should fail with an exception.
            try {
                mAwContents.postMessageToFrame(null, "2", mWebServer.getBaseUrl(),
                        new AppWebMessagePort[]{channel[1]});
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // There are two cases that put a port in a started state.
    // 1. posting a message
    // 2. setting an event handler.
    // A started port cannot return to "non-started" state. The four tests below verifies
    // these conditions for both conditions, using message ports and message channels.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testStartedPortCannotBeTransferredUsingPostMessageToFrame1() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channel[1].postMessage("1", null);
            try {
                mAwContents.postMessageToFrame(null, "2", mWebServer.getBaseUrl(),
                        new AppWebMessagePort[]{channel[1]});
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // see documentation in testStartedPortCannotBeTransferredUsingPostMessageToFrame1
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testStartedPortCannotBeTransferredUsingPostMessageToFrame2() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            // set a web event handler, this puts the port in a started state.
            channel[1].setMessageCallback((message, sentPorts) -> {
            }, null);
            try {
                mAwContents.postMessageToFrame(null, "2", mWebServer.getBaseUrl(),
                        new AppWebMessagePort[]{channel[1]});
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // see documentation in testStartedPortCannotBeTransferredUsingPostMessageToFrame1
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    @RetryOnFailure
    public void testStartedPortCannotBeTransferredUsingMessageChannel1() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel1 = mAwContents.createMessageChannel();
            channel1[1].postMessage("1", null);
            AppWebMessagePort[] channel2 = mAwContents.createMessageChannel();
            try {
                channel2[0].postMessage("2", new AppWebMessagePort[]{channel1[1]});
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // see documentation in testStartedPortCannotBeTransferredUsingPostMessageToFrame1
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testStartedPortCannotBeTransferredUsingMessageChannel2() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel1 = mAwContents.createMessageChannel();
            // set a web event handler, this puts the port in a started state.
            channel1[1].setMessageCallback((message, sentPorts) -> {
            }, null);
            AppWebMessagePort[] channel2 = mAwContents.createMessageChannel();
            try {
                channel2[0].postMessage("1", new AppWebMessagePort[]{channel1[1]});
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }


    // channel[0] and channel[1] are entangled ports, establishing a channel. Verify
    // it is not allowed to transfer channel[0] on channel[0].postMessage.
    // TODO(sgurun) Note that the related case of posting channel[1] via
    // channel[0].postMessage does not throw a JS exception at present. We do not throw
    // an exception in this case either since the information of entangled port is not
    // available at the source port. We need a new mechanism to implement to prevent
    // this case.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferringSourcePortViaMessageChannelNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            try {
                channel[0].postMessage("1", new AppWebMessagePort[]{channel[0]});
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // Verify a closed port cannot be transferred to a frame.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testSendClosedPortToFrameNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channel[1].close();
            try {
                mAwContents.postMessageToFrame(null, "1", mWebServer.getBaseUrl(),
                        new AppWebMessagePort[]{channel[1]});
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // Verify a closed port cannot be transferred to a port.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testSendClosedPortToPortNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel1 = mAwContents.createMessageChannel();
            AppWebMessagePort[] channel2 = mAwContents.createMessageChannel();
            channel2[1].close();
            try {
                channel1[0].postMessage("1", new AppWebMessagePort[]{channel2[1]});
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // Verify messages cannot be posted to closed ports.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToClosedPortNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channel[0].close();
            try {
                channel[0].postMessage("1", null);
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // Verify messages posted before closing a port is received at the destination port.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessagesPostedBeforeClosingPortAreTransferred() throws Throwable {
        loadPage(TITLE_FROM_POSTMESSAGE_TO_CHANNEL);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            mAwContents.postMessageToFrame(
                    null, "1", mWebServer.getBaseUrl(), new AppWebMessagePort[]{channel[1]});
            channel[0].postMessage("2", null);
            channel[0].postMessage("3", null);
            channel[0].close();
        });
        expectTitle("23");
    }

    // Verify a transferred port using postmessagetoframe cannot be closed.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testClosingTransferredPortToFrameThrowsAnException() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            mAwContents.postMessageToFrame(
                    null, "1", mWebServer.getBaseUrl(), new AppWebMessagePort[]{channel[1]});
            try {
                channel[1].close();
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // Verify a transferred port using postmessagetoframe cannot be closed.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testClosingTransferredPortToChannelThrowsAnException() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel1 = mAwContents.createMessageChannel();
            mAwContents.postMessageToFrame(
                    null, "1", mWebServer.getBaseUrl(), new AppWebMessagePort[]{channel1[1]});
            AppWebMessagePort[] channel2 = mAwContents.createMessageChannel();
            channel1[0].postMessage("2", new AppWebMessagePort[]{channel2[0]});
            try {
                channel2[0].close();
            } catch (IllegalStateException ex) {
                latch.countDown();
                return;
            }
            Assert.fail();
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // Create two message channels, and while they are in pending state, transfer the
    // second one in the first one.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPendingPortCanBeTransferredInPendingPort() throws Throwable {
        loadPage(TITLE_FROM_POSTMESSAGE_TO_CHANNEL);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel1 = mAwContents.createMessageChannel();
            mAwContents.postMessageToFrame(
                    null, "1", mWebServer.getBaseUrl(), new AppWebMessagePort[]{channel1[1]});
            AppWebMessagePort[] channel2 = mAwContents.createMessageChannel();
            channel1[0].postMessage("2", new AppWebMessagePort[]{channel2[0]});
        });
        expectTitle("2");
    }

    private static class ChannelContainer {
        private boolean mReady;
        private AppWebMessagePort[] mChannel;
        private final Object mLock = new Object();
        private String mMessage = "";
        private Looper mLastLooper;
        private int mCount;
        private int mWaitCount;

        public ChannelContainer() {
            this(1);
        }

        public ChannelContainer(int n) {
            mWaitCount = n;
        }

        public void set(AppWebMessagePort[] channel) {
            mChannel = channel;
        }
        public AppWebMessagePort[] get() {
            return mChannel;
        }

        public void setMessage(String message) {
            synchronized (mLock) {
                mMessage += message;
                mLastLooper = Looper.myLooper();
                if (++mCount < mWaitCount) return;
                mReady = true;
                mLock.notify();
            }
        }

        public int getMessageCount() {
            return mCount;
        }

        public String getMessage() {
            return mMessage;
        }

        public Looper getLastLooper() {
            return mLastLooper;
        }

        public void waitForMessage() throws InterruptedException {
            synchronized (mLock) {
                long deadline = System.currentTimeMillis() + TIMEOUT;
                while (!mReady && System.currentTimeMillis() < deadline) {
                    mLock.wait(deadline - System.currentTimeMillis());
                }
            }
        }
    }

    // Verify that messages from JS can be waited on a UI thread.
    // TODO(sgurun) this test turned out to be flaky. When it fails, it always fails in IPC.
    // When a postmessage is received, an IPC message is sent from browser to renderer
    // to convert the postmessage from WebSerializedScriptValue to a string. The IPC is sent
    // and seems to be received by IPC in renderer, but then nothing else seems to happen.
    // The issue seems like blocking the UI thread causes a racing SYNC ipc from renderer
    // to browser to block waiting for UI thread, and this would in turn block renderer
    // doing the conversion.
    @Test
    @DisabledTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testReceiveMessageInBackgroundThread() throws Throwable {
        loadPage(TEST_PAGE);
        final ChannelContainer channelContainer = new ChannelContainer();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            // verify communication from JS to Java.
            channelContainer.set(channel);
            channel[0].setMessageCallback(
                    (message, sentPorts) -> channelContainer.setMessage(message), null);
            mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                    new AppWebMessagePort[]{channel[1]});
        });
        mMessageObject.waitForMessage();
        Assert.assertEquals(WEBVIEW_MESSAGE, mMessageObject.getData());
        Assert.assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
        // verify that one message port is received at the js side
        Assert.assertEquals(1, mMessageObject.getPorts().length);
        // wait until we receive a message from JS
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            try {
                channelContainer.waitForMessage();
            } catch (InterruptedException e) {
                // ignore.
            }
        });
        Assert.assertEquals(JS_MESSAGE, channelContainer.getMessage());
    }

    private static final String ECHO_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        onmessage = function (e) {"
            + "            var myPort = e.ports[0];"
            + "            myPort.onmessage = function(e) {"
            + "                myPort.postMessage(e.data + \"" + JS_MESSAGE + "\"); }"
            + "        }"
            + "   </script>"
            + "</body></html>";

    private static final String HELLO = "HELLO";

    // Message channels are created on UI thread. Verify that a message port
    // can be transferred to JS and full communication can happen on it. Do
    // this by sending a message to JS and letting it echo the message with
    // some text prepended to it.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelUsingInitializedPort() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        final AppWebMessagePort[] channel =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            channel[0].setMessageCallback(
                    (message, sentPorts) -> channelContainer.setMessage(message), null);
            mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                    new AppWebMessagePort[]{channel[1]});
            channel[0].postMessage(HELLO, null);
        });
        // wait for the asynchronous response from JS
        channelContainer.waitForMessage();
        Assert.assertEquals(HELLO + JS_MESSAGE, channelContainer.getMessage());
    }

    // Verify that a message port can be used immediately (even if it is in
    // pending state) after creation. In particular make sure the message port can be
    // transferred to JS and full communication can happen on it.
    // Do this by sending a message to JS and let it echo'ing the message with
    // some text prepended to it.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    @Test
    public void testMessageChannelUsingPendingPort() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channel[0].setMessageCallback(
                    (message, sentPorts) -> channelContainer.setMessage(message), null);
            mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                    new AppWebMessagePort[]{channel[1]});
            channel[0].postMessage(HELLO, null);
        });
        // Wait for the asynchronous response from JS.
        channelContainer.waitForMessage();
        Assert.assertEquals(HELLO + JS_MESSAGE, channelContainer.getMessage());
    }

    // Verify that a message port can be used for message transfer when both
    // ports are owned by same Webview.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelCommunicationWithinWebView() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channel[1].setMessageCallback(
                    (message, sentPorts) -> channelContainer.setMessage(message), null);
            channel[0].postMessage(HELLO, null);
        });
        // Wait for the asynchronous response from JS.
        channelContainer.waitForMessage();
        Assert.assertEquals(HELLO, channelContainer.getMessage());
    }

    // Post a message with a pending port to a frame and then post a bunch of messages
    // after that. Make sure that they are not ordered at the receiver side.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToFrameNotReordersMessages() throws Throwable {
        loadPage(TITLE_FROM_POSTMESSAGE_TO_FRAME);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            mAwContents.postMessageToFrame(
                    null, "1", mWebServer.getBaseUrl(), new AppWebMessagePort[]{channel[1]});
            mAwContents.postMessageToFrame(null, "2", mWebServer.getBaseUrl(), null);
            mAwContents.postMessageToFrame(null, "3", mWebServer.getBaseUrl(), null);
        });
        expectTitle("123");
    }

    private static final String RECEIVE_JS_MESSAGE_CHANNEL_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        var received ='';"
            + "        var mc = new MessageChannel();"
            + "        mc.port1.onmessage = function (e) {"
            + "            received += e.data;"
            + "            document.title = received;"
            + "            if (e.data == '2') { mc.port1.postMessage('3'); }"
            + "        };"
            + "        onmessage = function (e) {"
            + "            var myPort = e.ports[0];"
            + "            myPort.postMessage('from window', [mc.port2]);"
            + "        }"
            + "   </script>"
            + "</body></html>";

    // Test webview can use a message port received from JS for full duplex communication.
    // Test steps:
    // 1. Java creates a message channel, and send one port to JS
    // 2. JS creates a new message channel and sends one port to Java using the channel in 1
    // 3. Java sends a message using the new channel in 2.
    // 4. Js responds to this message using the channel in 2.
    // 5. Java responds to message in 4 using the channel in 2.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    @Test
    public void testCanUseReceivedAwMessagePortFromJS() throws Throwable {
        loadPage(RECEIVE_JS_MESSAGE_CHANNEL_PAGE);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            mAwContents.postMessageToFrame(
                    null, "1", mWebServer.getBaseUrl(), new AppWebMessagePort[]{channel[1]});
            channel[0].setMessageCallback((message, p) -> {
                p[0].setMessageCallback((message1, q) -> {
                    Assert.assertEquals("3", message1);
                    p[0].postMessage("4", null);
                }, null);
                p[0].postMessage("2", null);
            }, null);
        });
        expectTitle("24");
    }

    private static final String WORKER_MESSAGE = "from_worker";

    // Listen for messages. Pass port 1 to worker and use port 2 to receive messages from
    // from worker.
    private static final String TEST_PAGE_FOR_PORT_TRANSFER =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        var worker = new Worker(\"worker.js\");"
            + "        onmessage = function (e) {"
            + "            if (e.data == \"" + WEBVIEW_MESSAGE + "\") {"
            + "                worker.postMessage(\"worker_port\", [e.ports[0]]);"
            + "                var messageChannelPort = e.ports[1];"
            + "                messageChannelPort.onmessage = receiveWorkerMessage;"
            + "            }"
            + "        };"
            + "        function receiveWorkerMessage(e) {"
            + "            if (e.data == \"" + WORKER_MESSAGE + "\") {"
            + "                messageObject.setMessageParams(e.data, e.origin, e.ports);"
            + "            }"
            + "        };"
            + "   </script>"
            + "</body></html>";

    private static final String WORKER_SCRIPT =
            "onmessage = function(e) {"
            + "    if (e.data == \"worker_port\") {"
            + "        var toWindow = e.ports[0];"
            + "        toWindow.postMessage(\"" + WORKER_MESSAGE + "\");"
            + "        toWindow.start();"
            + "    }"
            + "}";

    // Test if message ports created at the native side can be transferred
    // to JS side, to establish a communication channel between a worker and a frame.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    @RetryOnFailure
    public void testTransferPortsToWorker() throws Throwable {
        mWebServer.setResponse("/worker.js", WORKER_SCRIPT,
                CommonResources.getTextJavascriptHeaders(true));
        loadPage(TEST_PAGE_FOR_PORT_TRANSFER);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                    new AppWebMessagePort[]{channel[0], channel[1]});
        });
        mMessageObject.waitForMessage();
        Assert.assertEquals(WORKER_MESSAGE, mMessageObject.getData());
    }

    private static final String POPUP_MESSAGE = "from_popup";
    private static final String POPUP_URL = "/popup.html";
    private static final String IFRAME_URL = "/iframe.html";
    private static final String MAIN_PAGE_FOR_POPUP_TEST = "<!DOCTYPE html><html>"
            + "<head>"
            + "    <script>"
            + "        function createPopup() {"
            + "            var popupWindow = window.open('" + POPUP_URL + "');"
            + "            onmessage = function(e) {"
            + "                popupWindow.postMessage(e.data, '*', e.ports);"
            + "            };"
            + "        }"
            + "    </script>"
            + "</head>"
            + "</html>";

    // Sends message and ports to the iframe.
    private static final String POPUP_PAGE_WITH_IFRAME = "<!DOCTYPE html><html>"
            + "<script>"
            + "    onmessage = function(e) {"
            + "        var iframe = document.getElementsByTagName('iframe')[0];"
            + "        iframe.contentWindow.postMessage('" + POPUP_MESSAGE + "', '*', e.ports);"
            + "    };"
            + "</script>"
            + "<body><iframe src='" + IFRAME_URL + "'></iframe></body>"
            + "</html>";

    // Test if WebView can post a message from/to a popup window owning a message port.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToPopup() throws Throwable {
        mActivityTestRule.triggerPopup(mAwContents, mContentsClient, mWebServer,
                MAIN_PAGE_FOR_POPUP_TEST, ECHO_PAGE, POPUP_URL, "createPopup()");
        mActivityTestRule.connectPendingPopup(mAwContents);
        final ChannelContainer channelContainer = new ChannelContainer();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channelContainer.set(channel);
            channel[0].setMessageCallback(
                    (message, sentPorts) -> channelContainer.setMessage(message), null);
            mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                    new AppWebMessagePort[]{channel[1]});
            channel[0].postMessage(HELLO, null);
        });
        channelContainer.waitForMessage();
        Assert.assertEquals(HELLO + JS_MESSAGE, channelContainer.getMessage());
    }

    // Test if WebView can post a message from/to an iframe in a popup window.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToIframeInsidePopup() throws Throwable {
        mWebServer.setResponse(IFRAME_URL, ECHO_PAGE, null);
        mActivityTestRule.triggerPopup(mAwContents, mContentsClient, mWebServer,
                MAIN_PAGE_FOR_POPUP_TEST, POPUP_PAGE_WITH_IFRAME, POPUP_URL, "createPopup()");
        mActivityTestRule.connectPendingPopup(mAwContents);
        final ChannelContainer channelContainer = new ChannelContainer();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channelContainer.set(channel);
            channel[0].setMessageCallback(
                    (message, sentPorts) -> channelContainer.setMessage(message), null);
            mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                    new AppWebMessagePort[]{channel[1]});
            channel[0].postMessage(HELLO, null);
        });
        channelContainer.waitForMessage();
        Assert.assertEquals(HELLO + JS_MESSAGE, channelContainer.getMessage());
    }

    private static final String TEST_PAGE_FOR_UNSUPPORTED_MESSAGES = "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        onmessage = function (e) {"
            + "            e.ports[0].postMessage(null);"
            + "            e.ports[0].postMessage(undefined);"
            + "            e.ports[0].postMessage(NaN);"
            + "            e.ports[0].postMessage(0);"
            + "            e.ports[0].postMessage(new Set());"
            + "            e.ports[0].postMessage({});"
            + "            e.ports[0].postMessage(['1','2','3']);"
            + "            e.ports[0].postMessage('" + JS_MESSAGE + "');"
            + "        }"
            + "   </script>"
            + "</body></html>";

    // Make sure that postmessage can handle unsupported messages gracefully.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostUnsupportedWebMessageToApp() throws Throwable {
        loadPage(TEST_PAGE_FOR_UNSUPPORTED_MESSAGES);
        final ChannelContainer channelContainer = new ChannelContainer();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channelContainer.set(channel);
            channel[0].setMessageCallback(
                    (message, sentPorts) -> channelContainer.setMessage(message), null);
            mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                    new AppWebMessagePort[]{channel[1]});
        });
        channelContainer.waitForMessage();
        Assert.assertEquals(JS_MESSAGE, channelContainer.getMessage());
        // Assert that onMessage is called only once.
        Assert.assertEquals(1, channelContainer.getMessageCount());
    }

    private static final String TEST_TRANSFER_EMPTY_PORTS = "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        onmessage = function (e) {"
            + "            e.ports[0].postMessage('1', undefined);"
            + "            e.ports[0].postMessage('2', []);"
            + "        }"
            + "   </script>"
            + "</body></html>";

    // Make sure that postmessage can handle unsupported messages gracefully.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferEmptyPortsArray() throws Throwable {
        loadPage(TEST_TRANSFER_EMPTY_PORTS);
        final ChannelContainer channelContainer = new ChannelContainer(2);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channelContainer.set(channel);
            channel[0].setMessageCallback(
                    (message, sentPorts) -> channelContainer.setMessage(message), null);
            mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                    new AppWebMessagePort[]{channel[1]});
        });
        channelContainer.waitForMessage();
        Assert.assertEquals("12", channelContainer.getMessage());
    }

    // Make sure messages are dispatched on the correct looper.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageOnCorrectLooper() throws Throwable {
        final ChannelContainer channelContainer1 = new ChannelContainer(1);
        final ChannelContainer channelContainer2 = new ChannelContainer(1);
        final HandlerThread thread = new HandlerThread("test-thread");
        thread.start();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channelContainer1.set(channel);
            channelContainer2.set(channel);
            channel[0].setMessageCallback(
                    (message, sentPorts) -> channelContainer1.setMessage(message), null);
            channel[1].setMessageCallback((message, sentPorts)
                                                  -> channelContainer2.setMessage(message),
                    new Handler(thread.getLooper()));
            channel[0].postMessage("foo", null);
            channel[1].postMessage("bar", null);
        });
        channelContainer1.waitForMessage();
        channelContainer2.waitForMessage();
        Assert.assertEquals("bar", channelContainer1.getMessage());
        Assert.assertEquals(Looper.getMainLooper(), channelContainer1.getLastLooper());
        Assert.assertEquals("foo", channelContainer2.getMessage());
        Assert.assertEquals(thread.getLooper(), channelContainer2.getLastLooper());
    }

    // Make sure it is possible to change the message handler.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testChangeMessageHandler() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer(1);
        final HandlerThread thread = new HandlerThread("test-thread");
        thread.start();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = mAwContents.createMessageChannel();
            channelContainer.set(channel);
            channel[0].setMessageCallback((message, sentPorts)
                                                  -> channelContainer.setMessage(message),
                    new Handler(thread.getLooper()));
            channel[1].postMessage("foo", null);
        });
        channelContainer.waitForMessage();
        Assert.assertEquals("foo", channelContainer.getMessage());
        Assert.assertEquals(thread.getLooper(), channelContainer.getLastLooper());
        final ChannelContainer channelContainer2 = new ChannelContainer(1);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AppWebMessagePort[] channel = channelContainer.get();
            channel[0].setMessageCallback(
                    (message, sentPorts) -> channelContainer2.setMessage(message), null);
            channel[1].postMessage("bar", null);
        });
        channelContainer2.waitForMessage();
        Assert.assertEquals("bar", channelContainer2.getMessage());
        Assert.assertEquals(Looper.getMainLooper(), channelContainer2.getLastLooper());
    }
}
