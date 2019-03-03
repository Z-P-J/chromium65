// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password Settings tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
const ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

// Fake data generator.
GEN_INCLUDE([ROOT_PATH +
    'chrome/test/data/webui/settings/passwords_and_autofill_fake_data.js']);

/**
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsPasswordSectionBrowserTest() {}

SettingsPasswordSectionBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload:
      'chrome://settings/passwords_and_forms_page/passwords_section.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'ensure_lazy_loaded.js',
  ]),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // Test is run on an individual element that won't have a page language.
    this.accessibilityAuditConfig.auditRulesToIgnore.push('humanLangMissing');

    settings.ensureLazyLoaded();
  },
};

/** This test will validate that the section is loaded with data. */
TEST_F('SettingsPasswordSectionBrowserTest', 'uiTests', function() {
  /**
   * Helper method that validates a that elements in the password list match
   * the expected data.
   * @param {!Element} listElement The iron-list element that will be checked.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList The
   *     expected data.
   * @private
   */
  function validatePasswordList(listElement, passwordList) {
    assertEquals(passwordList.length, listElement.items.length);
    if (passwordList.length > 0) {
      // The first child is a template, skip and get the real 'first child'.
      const node = Polymer.dom(listElement).children[1];
      assert(node);
      const passwordInfo = passwordList[0];
      assertEquals(passwordInfo.loginPair.urls.shown,
          node.$$('#originUrl').textContent.trim());
      assertEquals(passwordInfo.loginPair.urls.link,
          node.$$('#originUrl').href);
      assertEquals(
          passwordInfo.loginPair.username,
          node.$$('#username').textContent.trim());
      assertEquals(passwordInfo.numCharactersInPassword,
          node.$$('#password').value.length);
    }
  }

  /**
   * Helper method that validates a that elements in the exception list match
   * the expected data.
   * @param {!Array<!Element>} nodes The nodes that will be checked.
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList The
   *     expected data.
   * @private
   */
  function validateExceptionList(nodes, exceptionList) {
    assertEquals(exceptionList.length, nodes.length);
    for (let index = 0; index < exceptionList.length; ++index) {
      const node = nodes[index];
      const exception = exceptionList[index];
      assertEquals(
          exception.urls.shown,
          node.querySelector('#exception').textContent.trim());
      assertEquals(
          exception.urls.link.toLowerCase(),
          node.querySelector('#exception').href);
    }
  }

  /**
   * Returns all children of an element that has children added by a dom-repeat.
   * @param {!Element} element
   * @return {!Array<!Element>}
   * @private
   */
  function getDomRepeatChildren(element) {
    const nodes = element.querySelectorAll('.list-item:not([id])');
    return nodes;
  }

  /**
   * Allow the iron-list to be sized properly.
   * @param {!Object} passwordsSection
   * @private
   */
  function flushPasswordSection(passwordsSection) {
    passwordsSection.$.passwordList.notifyResize();
    Polymer.dom.flush();
  }

  /**
   * Helper method used to create a password section for the given lists.
   * @param {!PasswordManager} passwordManager
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList
   * @return {!Object}
   * @private
   */
  function createPasswordsSection(passwordManager, passwordList,
      exceptionList) {
    // Override the PasswordManager data for testing.
    passwordManager.data.passwords = passwordList;
    passwordManager.data.exceptions = exceptionList;

    // Create a passwords-section to use for testing.
    const passwordsSection = document.createElement('passwords-section');
    document.body.appendChild(passwordsSection);
    flushPasswordSection(passwordsSection);
    return passwordsSection;
  }

  /**
   * Helper method used to create a password list item.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} passwordItem
   * @return {!Object}
   * @private
   */
  function createPasswordListItem(passwordItem) {
    const passwordListItem = document.createElement('password-list-item');
    passwordListItem.item = {entry: passwordItem, password: ''};
    document.body.appendChild(passwordListItem);
    Polymer.dom.flush();
    return passwordListItem;
  }

  /**
   * Helper method used to create a password editing dialog.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} passwordItem
   * @return {!Object}
   * @private
   */
  function createPasswordDialog(passwordItem) {
    const passwordDialog = document.createElement('password-edit-dialog');
    passwordDialog.item = {entry: passwordItem, password: ''};
    document.body.appendChild(passwordDialog);
    Polymer.dom.flush();
    return passwordDialog;
  }

  /**
   * Helper method used to create an export passwords dialog.
   * @return {!Object}
   * @private
   */
  function createExportPasswordsDialog() {
    const dialog = document.createElement('passwords-export-dialog');
    document.body.appendChild(dialog);
    Polymer.dom.flush();
    return dialog;
  }

  /**
   * Helper method used to test for a url in a list of passwords.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {string} url The URL that is being searched for.
   */
  function listContainsUrl(passwordList, url) {
    for (let i = 0; i < passwordList.length; ++i) {
      if (passwordList[i].loginPair.urls.origin == url)
        return true;
    }
    return false;
  }

  /**
   * Helper method used to test for a url in a list of passwords.
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList
   * @param {string} url The URL that is being searched for.
   */
  function exceptionsListContainsUrl(exceptionList, url) {
    for (let i = 0; i < exceptionList.length; ++i) {
      if (exceptionList[i].urls.orginUrl == url)
        return true;
    }
    return false;
  }

  suite('PasswordsSection', function() {
    /** @type {TestPasswordManager} */
    let passwordManager = null;

    setup(function() {
      PolymerTest.clearBody();
      // Override the PasswordManagerImpl for testing.
      passwordManager = new TestPasswordManager();
      PasswordManagerImpl.instance_ = passwordManager;
    });

    test('testPasswordsExtensionIndicator', function() {
      // Initialize with dummy prefs.
      const element = document.createElement('passwords-section');
      element.prefs = {credentials_enable_service: {}};
      document.body.appendChild(element);

      assertFalse(!!element.$$('#passwordsExtensionIndicator'));
      element.set('prefs.credentials_enable_service.extensionId', 'test-id');
      Polymer.dom.flush();

      assertTrue(!!element.$$('#passwordsExtensionIndicator'));
    });

    test('verifyNoSavedPasswords', function() {
      const passwordsSection = createPasswordsSection(passwordManager, [], []);

      validatePasswordList(passwordsSection.$.passwordList, []);

      assertFalse(passwordsSection.$.noPasswordsLabel.hidden);
      assertTrue(passwordsSection.$.savedPasswordsHeading.hidden);
    });

    test('verifySavedPasswordLength', function() {
      const passwordList = [
        FakeDataMaker.passwordEntry('site1.com', 'luigi', 1),
        FakeDataMaker.passwordEntry('longwebsite.com', 'peach', 7),
        FakeDataMaker.passwordEntry('site2.com', 'mario', 70),
        FakeDataMaker.passwordEntry('site1.com', 'peach', 11),
        FakeDataMaker.passwordEntry('google.com', 'mario', 7),
        FakeDataMaker.passwordEntry('site2.com', 'luigi', 8),
      ];

      const passwordsSection = createPasswordsSection(
          passwordManager, passwordList, []);

      // Assert that the data is passed into the iron list. If this fails,
      // then other expectations will also fail.
      assertDeepEquals(
          passwordList,
          passwordsSection.$.passwordList.items.map(entry => entry.entry));

      validatePasswordList(passwordsSection.$.passwordList, passwordList);

      assertTrue(passwordsSection.$.noPasswordsLabel.hidden);
      assertFalse(passwordsSection.$.savedPasswordsHeading.hidden);
    });

    // Test verifies that removing a password will update the elements.
    test('verifyPasswordListRemove', function() {
      const passwordList = [
        FakeDataMaker.passwordEntry('anotherwebsite.com', 'luigi', 1),
        FakeDataMaker.passwordEntry('longwebsite.com', 'peach', 7),
        FakeDataMaker.passwordEntry('website.com', 'mario', 70)
      ];

      const passwordsSection = createPasswordsSection(
          passwordManager, passwordList, []);

      validatePasswordList(passwordsSection.$.passwordList, passwordList);
      // Simulate 'longwebsite.com' being removed from the list.
      passwordsSection.splice('savedPasswords', 1, 1);
      passwordList.splice(1, 1);
      flushPasswordSection(passwordsSection);

      assertFalse(listContainsUrl(
          passwordsSection.savedPasswords.map(entry => entry.entry),
          'longwebsite.com'));
      assertFalse(listContainsUrl(passwordList, 'longwebsite.com'));

      validatePasswordList(passwordsSection.$.passwordList, passwordList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any passwords.
    test('verifyPasswordItemRemoveButton', function(done) {
      const passwordList = [
        FakeDataMaker.passwordEntry('one', 'six', 5),
        FakeDataMaker.passwordEntry('two', 'five', 3),
        FakeDataMaker.passwordEntry('three', 'four', 1),
        FakeDataMaker.passwordEntry('four', 'three', 2),
        FakeDataMaker.passwordEntry('five', 'two', 4),
        FakeDataMaker.passwordEntry('six', 'one', 6),
      ];

      const passwordsSection = createPasswordsSection(
          passwordManager, passwordList, []);

      // The first child is a template, skip and get the real 'first child'.
      const firstNode =
          Polymer.dom(passwordsSection.$.passwordList).children[1];
      assert(firstNode);
      const firstPassword = passwordList[0];

      passwordManager.onRemoveSavedPassword = function(index) {
        // Verify that the event matches the expected value.
        assertEquals(firstPassword.index, index);

        // Clean up after self.
        passwordManager.onRemoveSavedPassword = null;

        done();
      };

      // Click the remove button on the first password.
      MockInteractions.tap(firstNode.$$('#passwordMenu'));
      MockInteractions.tap(passwordsSection.$.menuRemovePassword);
    });

    test('verifyFilterPasswords', function() {
      const passwordList = [
        FakeDataMaker.passwordEntry('one.com', 'SHOW', 5),
        FakeDataMaker.passwordEntry('two.com', 'shower', 3),
        FakeDataMaker.passwordEntry('three.com/show', 'four', 1),
        FakeDataMaker.passwordEntry('four.com', 'three', 2),
        FakeDataMaker.passwordEntry('five.com', 'two', 4),
        FakeDataMaker.passwordEntry('six-show.com', 'one', 6),
      ];

      const passwordsSection = createPasswordsSection(
          passwordManager, passwordList, []);
      passwordsSection.filter = 'SHow';
      Polymer.dom.flush();

      const expectedList = [
        FakeDataMaker.passwordEntry('one.com', 'SHOW', 5),
        FakeDataMaker.passwordEntry('two.com', 'shower', 3),
        FakeDataMaker.passwordEntry('three.com/show', 'four', 1),
        FakeDataMaker.passwordEntry('six-show.com', 'one', 6),
      ];

      validatePasswordList(passwordsSection.$.passwordList, expectedList);
    });

    test('verifyFilterPasswordExceptions', function() {
      const exceptionList = [
        FakeDataMaker.exceptionEntry('docsshoW.google.com'),
        FakeDataMaker.exceptionEntry('showmail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('mapsshow.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.comshow'),
      ];

      const passwordsSection = createPasswordsSection(
          passwordManager, [], exceptionList);
      passwordsSection.filter = 'shOW';
      Polymer.dom.flush();

      const expectedExceptionList = [
        FakeDataMaker.exceptionEntry('docsshoW.google.com'),
        FakeDataMaker.exceptionEntry('showmail.com'),
        FakeDataMaker.exceptionEntry('mapsshow.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.comshow'),
      ];

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          expectedExceptionList);
    });

    test('verifyNoPasswordExceptions', function() {
      const passwordsSection = createPasswordsSection(passwordManager, [], []);

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          []);

      assertFalse(passwordsSection.$.noExceptionsLabel.hidden);
    });

    test('verifyPasswordExceptions', function() {
      const exceptionList = [
        FakeDataMaker.exceptionEntry('docs.google.com'),
        FakeDataMaker.exceptionEntry('mail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('maps.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.com'),
      ];

      const passwordsSection = createPasswordsSection(
          passwordManager, [], exceptionList);

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          exceptionList);

      assertTrue(passwordsSection.$.noExceptionsLabel.hidden);
    });

    // Test verifies that removing an exception will update the elements.
    test('verifyPasswordExceptionRemove', function() {
      const exceptionList = [
        FakeDataMaker.exceptionEntry('docs.google.com'),
        FakeDataMaker.exceptionEntry('mail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('maps.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.com'),
      ];

      const passwordsSection = createPasswordsSection(
          passwordManager, [], exceptionList);

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          exceptionList);

      // Simulate 'mail.com' being removed from the list.
      passwordsSection.splice('passwordExceptions', 1, 1);
      assertFalse(exceptionsListContainsUrl(
            passwordsSection.passwordExceptions, 'mail.com'));
      assertFalse(exceptionsListContainsUrl(exceptionList, 'mail.com'));
      flushPasswordSection(passwordsSection);

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          exceptionList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any exceptions.
    test('verifyPasswordExceptionRemoveButton', function(done) {
      const exceptionList = [
        FakeDataMaker.exceptionEntry('docs.google.com'),
        FakeDataMaker.exceptionEntry('mail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('maps.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.com'),
      ];

      const passwordsSection = createPasswordsSection(
          passwordManager, [], exceptionList);

      const exceptions =
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList);

      // The index of the button currently being checked.
      let item = 0;

      const clickRemoveButton = function() {
        MockInteractions.tap(
            exceptions[item].querySelector('#removeExceptionButton'));
      };

      passwordManager.onRemoveException = function(index) {
        // Verify that the event matches the expected value.
        assertTrue(item < exceptionList.length);
        assertEquals(index, exceptionList[item].index);

        if (++item < exceptionList.length) {
          clickRemoveButton();  // Click 'remove' on all passwords, one by one.
        } else {
          // Clean up after self.
          passwordManager.onRemoveException = null;

          done();
        }
      };

      // Start removing.
      clickRemoveButton();
    });

    test('verifyFederatedPassword', function() {
      const item = FakeDataMaker.passwordEntry('goo.gl', 'bart', 0);
      item.federationText = 'with chromium.org';
      const passwordDialog = createPasswordDialog(item);

      Polymer.dom.flush();

      assertEquals(item.federationText,
                   passwordDialog.$.passwordInput.value);
      // Text should be readable.
      assertEquals('text',
                   passwordDialog.$.passwordInput.type);
      assertTrue(passwordDialog.$.showPasswordButton.hidden);
    });

    test('showSavedPasswordEditDialog', function() {
      const PASSWORD = 'bAn@n@5';
      const item = FakeDataMaker.passwordEntry(
          'goo.gl', 'bart', PASSWORD.length);
      const passwordDialog = createPasswordDialog(item);

      assertFalse(passwordDialog.$.showPasswordButton.hidden);

      passwordDialog.set('item.password', PASSWORD);
      Polymer.dom.flush();

      assertEquals(PASSWORD,
                   passwordDialog.$.passwordInput.value);
      // Password should be visible.
      assertEquals('text',
                   passwordDialog.$.passwordInput.type);
      assertFalse(passwordDialog.$.showPasswordButton.hidden);
    });

    test('showSavedPasswordListItem', function() {
      const PASSWORD = 'bAn@n@5';
      const item =
          FakeDataMaker.passwordEntry('goo.gl', 'bart', PASSWORD.length);
      const passwordListItem = createPasswordListItem(item);
      // Hidden passwords should be disabled.
      assertTrue(passwordListItem.$$('#password').disabled);

      passwordListItem.set('item.password', PASSWORD);
      Polymer.dom.flush();

      assertEquals(PASSWORD, passwordListItem.$$('#password').value);
      // Password should be visible.
      assertEquals('text', passwordListItem.$$('#password').type);
      // Visible passwords should not be disabled.
      assertFalse(passwordListItem.$$('#password').disabled);

      // Hide Password Button should be shown.
      assertTrue(passwordListItem.$$('#showPasswordButton')
                     .classList.contains('icon-visibility-off'));
    });

    // Test will timeout if event is not received.
    test('onShowSavedPasswordEditDialog', function(done) {
      const expectedItem = FakeDataMaker.passwordEntry('goo.gl', 'bart', 1);
      const passwordDialog = createPasswordDialog(expectedItem);

      passwordDialog.addEventListener('show-password', function(event) {
        const actualItem = event.detail.item;
        assertEquals(
            expectedItem.loginPair.urls.origin,
            actualItem.entry.loginPair.urls.origin);
        assertEquals(
            expectedItem.loginPair.username,
            actualItem.entry.loginPair.username);
        done();
      });

      MockInteractions.tap(passwordDialog.$.showPasswordButton);
    });

    test('onShowSavedPasswordListItem', function(done) {
      const expectedItem = FakeDataMaker.passwordEntry('goo.gl', 'bart', 1);
      const passwordListItem = createPasswordListItem(expectedItem);

      passwordListItem.addEventListener('show-password', function(event) {
        const actualItem = event.detail.item;
        assertEquals(
            expectedItem.loginPair.urls.origin,
            actualItem.entry.loginPair.urls.origin);
        assertEquals(
            expectedItem.loginPair.username,
            actualItem.entry.loginPair.username);
        done();
      });

      MockInteractions.tap(passwordListItem.$$('#showPasswordButton'));
    });

    // Test that tapping "Export passwords..." notifies the browser accordingly
    test('startExport', function(done) {
      const exportDialog = createExportPasswordsDialog();

      passwordManager.exportPasswords = () => {
        done();
      };

      MockInteractions.tap(exportDialog.$.exportPasswordsButton);
    });

    test('closingPasswordsSectionHidesUndoToast', function(done) {
      const passwordEntry = FakeDataMaker.passwordEntry('goo.gl', 'bart', 1);
      const passwordsSection =
          createPasswordsSection(passwordManager, [passwordEntry], []);

      // Click the remove button on the first password and assert that an undo
      // toast is shown.
      const firstNode =
          Polymer.dom(passwordsSection.$.passwordList).children[1];
      MockInteractions.tap(firstNode.$$('#passwordMenu'));
      MockInteractions.tap(passwordsSection.$.menuRemovePassword);
      assertTrue(passwordsSection.$.undoToast.open);

      // Remove the passwords section from the DOM and check that this closes
      // the undo toast.
      document.body.removeChild(passwordsSection);
      assertFalse(passwordsSection.$.undoToast.open);

      done();
    });
  });

  mocha.run();
});
