// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('wallpapers', function() {
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var ListSelectionController = cr.ui.ListSelectionController;

  /**
   * Create a selection controller for the wallpaper categories list.
   * @param {cr.ui.ListSelectionModel} selectionModel
   * @constructor
   * @extends {cr.ui.ListSelectionController}
   */
  function WallpaperCategoriesListSelectionController(selectionModel) {
    ListSelectionController.call(this, selectionModel);
  }

  WallpaperCategoriesListSelectionController.prototype = {
    __proto__: ListSelectionController.prototype,

    /**
     * @override
     */
    getIndexBefore: function(index) {
      return this.getIndexAbove(index);
    },

    /**
     * @override
     */
    getIndexAfter: function(index) {
      return this.getIndexBelow(index);
    },
  };

  /**
   * Create a new wallpaper categories list (horizontal list).
   * @constructor
   * @extends {cr.ui.List}
   */
  var WallpaperCategoriesList = cr.ui.define('list');

  WallpaperCategoriesList.prototype = {
    __proto__: List.prototype,

    /**
     * @override
     */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.selectionModel = new ListSingleSelectionModel();
      this.dataModel = new ArrayDataModel([]);

      // cr.ui.list calculates items in view port based on client height and
      // item height. However, categories list is displayed horizontally. So we
      // should not calculate visible items here. Sets autoExpands to true to
      // show every item in the list.
      this.autoExpands = true;

      var self = this;
      this.itemConstructor = function(entry) {
        var li = self.ownerDocument.createElement('li');
        cr.defineProperty(li, 'custom', cr.PropertyKind.BOOL_ATTR);
        li.custom = (entry == loadTimeData.getString('customCategoryLabel'));
        cr.defineProperty(li, 'lead', cr.PropertyKind.BOOL_ATTR);
        cr.defineProperty(li, 'selected', cr.PropertyKind.BOOL_ATTR);
        var div = self.ownerDocument.createElement('div');
        div.textContent = entry;
        li.appendChild(div);
        return li;
      };
    },

    /**
     * @override
     */
    createSelectionController: function(sm) {
      return new WallpaperCategoriesListSelectionController(assert(sm));
    },
  };

  return {WallpaperCategoriesList: WallpaperCategoriesList};
});
