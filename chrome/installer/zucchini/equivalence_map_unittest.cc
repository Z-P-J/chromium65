// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/equivalence_map.h"

#include <cstring>
#include <utility>
#include <vector>

#include "chrome/installer/zucchini/encoded_view.h"
#include "chrome/installer/zucchini/image_index.h"
#include "chrome/installer/zucchini/suffix_array.h"
#include "chrome/installer/zucchini/targets_affinity.h"
#include "chrome/installer/zucchini/test_disassembler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

// Make all references 2 bytes long.
constexpr offset_t kReferenceSize = 2;

// Creates and initialize an ImageIndex from |a| and with 2 types of references.
// The result is populated with |refs0| and |refs1|. |a| is expected to be a
// string literal valid for the lifetime of the object.
ImageIndex MakeImageIndexForTesting(const char* a,
                                    std::vector<Reference>&& refs0,
                                    std::vector<Reference>&& refs1) {
  TestDisassembler disasm(
      {kReferenceSize, TypeTag(0), PoolTag(0)}, std::move(refs0),
      {kReferenceSize, TypeTag(1), PoolTag(0)}, std::move(refs1),
      {kReferenceSize, TypeTag(2), PoolTag(1)}, {});

  ImageIndex image_index(
      ConstBufferView(reinterpret_cast<const uint8_t*>(a), std::strlen(a)));

  EXPECT_TRUE(image_index.Initialize(&disasm));
  return image_index;
}

std::vector<TargetsAffinity> MakeTargetsAffinitiesForTesting(
    const ImageIndex& old_image_index,
    const ImageIndex& new_image_index,
    const EquivalenceMap& equivalence_map) {
  std::vector<TargetsAffinity> target_affinities(old_image_index.PoolCount());
  for (const auto& old_pool_tag_and_targets : old_image_index.target_pools()) {
    PoolTag pool_tag = old_pool_tag_and_targets.first;
    target_affinities[pool_tag.value()].InferFromSimilarities(
        equivalence_map, old_pool_tag_and_targets.second.targets(),
        new_image_index.pool(pool_tag).targets());
  }
  return target_affinities;
}

}  // namespace

TEST(EquivalenceMapTest, GetTokenSimilarity) {
  ImageIndex old_index = MakeImageIndexForTesting(
      "ab1122334455", {{2, 0}, {4, 1}, {6, 2}, {8, 2}}, {{10, 3}});
  // Note: {4, 1} -> {6, 3} and {6, 2} -> {4, 1}, then result is sorted.
  ImageIndex new_index = MakeImageIndexForTesting(
      "a11b33224455", {{1, 0}, {4, 1}, {6, 3}, {8, 1}}, {{10, 2}});
  std::vector<TargetsAffinity> affinities = MakeTargetsAffinitiesForTesting(
      old_index, new_index,
      EquivalenceMap({{{0, 0, 1}, 1.0}, {{1, 3, 1}, 1.0}}));

  // Raw match.
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 0, 0));
  // Raw mismatch.
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 0, 1));
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 1, 0));

  // Type mismatch.
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 0, 1));
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 2, 0));
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 2, 10));
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 10, 1));

  // Reference strong match.
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 2, 1));
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 4, 6));

  // Reference weak match.
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 6, 4));
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 6, 8));
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 8, 4));

  // Weak match is not greater than strong match.
  EXPECT_LE(GetTokenSimilarity(old_index, new_index, affinities, 6, 4),
            GetTokenSimilarity(old_index, new_index, affinities, 2, 1));

  // Reference mismatch.
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 2, 4));
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 2, 6));
}

TEST(EquivalenceMapTest, GetEquivalenceSimilarity) {
  ImageIndex image_index =
      MakeImageIndexForTesting("abcdef1122", {{6, 0}}, {{8, 1}});
  std::vector<TargetsAffinity> affinities =
      MakeTargetsAffinitiesForTesting(image_index, image_index, {});

  // Sanity check. These are no-op with length-0 equivalences.
  EXPECT_EQ(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 0, 0}));
  EXPECT_EQ(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 3, 0}));
  EXPECT_EQ(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {3, 0, 0}));

  // Now examine larger equivalences.
  EXPECT_LT(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 0, 3}));
  EXPECT_GE(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 3, 3}));
  EXPECT_GE(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {3, 0, 3}));

  EXPECT_LT(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {6, 6, 4}));
}

TEST(EquivalenceMapTest, ExtendEquivalenceForward) {
  auto test_extend_forward =
      [](const ImageIndex old_index, const ImageIndex new_index,
         const EquivalenceCandidate& equivalence, double base_similarity) {
        return ExtendEquivalenceForward(
                   old_index, new_index,
                   MakeTargetsAffinitiesForTesting(old_index, new_index, {}),
                   equivalence, base_similarity)
            .eq;
      };

  EXPECT_EQ(Equivalence({0, 0, 0}),
            test_extend_forward(MakeImageIndexForTesting("", {}, {}),
                                MakeImageIndexForTesting("", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 0}),
            test_extend_forward(MakeImageIndexForTesting("banana", {}, {}),
                                MakeImageIndexForTesting("zzzz", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 6}),
            test_extend_forward(MakeImageIndexForTesting("banana", {}, {}),
                                MakeImageIndexForTesting("banana", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({2, 2, 4}),
            test_extend_forward(MakeImageIndexForTesting("banana", {}, {}),
                                MakeImageIndexForTesting("banana", {}, {}),
                                {{2, 2, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 6}),
            test_extend_forward(MakeImageIndexForTesting("bananaxx", {}, {}),
                                MakeImageIndexForTesting("bananayy", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 8}),
      test_extend_forward(MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                          MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                          {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 6}),
      test_extend_forward(MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                          MakeImageIndexForTesting("banana22", {}, {{6, 0}}),
                          {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 17}),
      test_extend_forward(MakeImageIndexForTesting("bananaxxpineapple", {}, {}),
                          MakeImageIndexForTesting("bananayypineapple", {}, {}),
                          {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({3, 0, 19}),
      test_extend_forward(
          MakeImageIndexForTesting("foobanana11xxpineapplexx", {{9, 0}}, {}),
          MakeImageIndexForTesting("banana11yypineappleyy", {{6, 0}}, {}),
          {{3, 0, 0}, 0.0}, 8.0));
}

TEST(EquivalenceMapTest, ExtendEquivalenceBackward) {
  auto test_extend_backward =
      [](const ImageIndex old_index, const ImageIndex new_index,
         const EquivalenceCandidate& equivalence, double base_similarity) {
        return ExtendEquivalenceBackward(
                   old_index, new_index,
                   MakeTargetsAffinitiesForTesting(old_index, new_index, {}),
                   equivalence, base_similarity)
            .eq;
      };

  EXPECT_EQ(Equivalence({0, 0, 0}),
            test_extend_backward(MakeImageIndexForTesting("", {}, {}),
                                 MakeImageIndexForTesting("", {}, {}),
                                 {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({6, 4, 0}),
            test_extend_backward(MakeImageIndexForTesting("banana", {}, {}),
                                 MakeImageIndexForTesting("zzzz", {}, {}),
                                 {{6, 4, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 6}),
            test_extend_backward(MakeImageIndexForTesting("banana", {}, {}),
                                 MakeImageIndexForTesting("banana", {}, {}),
                                 {{6, 6, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({2, 2, 6}),
            test_extend_backward(MakeImageIndexForTesting("xxbanana", {}, {}),
                                 MakeImageIndexForTesting("yybanana", {}, {}),
                                 {{8, 8, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 8}),
      test_extend_backward(MakeImageIndexForTesting("11banana", {{0, 0}}, {}),
                           MakeImageIndexForTesting("11banana", {{0, 0}}, {}),
                           {{8, 8, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({2, 2, 6}),
      test_extend_backward(MakeImageIndexForTesting("11banana", {{0, 0}}, {}),
                           MakeImageIndexForTesting("22banana", {}, {{0, 0}}),
                           {{8, 8, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 17}),
            test_extend_backward(
                MakeImageIndexForTesting("bananaxxpineapple", {}, {}),
                MakeImageIndexForTesting("bananayypineapple", {}, {}),
                {{8, 8, 9}, 9.0}, 8.0));

  EXPECT_EQ(
      Equivalence({3, 0, 19}),
      test_extend_backward(
          MakeImageIndexForTesting("foobanana11xxpineapplexx", {{9, 0}}, {}),
          MakeImageIndexForTesting("banana11yypineappleyy", {{6, 0}}, {}),
          {{22, 19, 0}, 0.0}, 8.0));
}

TEST(EquivalenceMapTest, Build) {
  auto test_build_equivalence = [](const ImageIndex old_index,
                                   const ImageIndex new_index,
                                   double minimum_similarity) {
    auto affinities = MakeTargetsAffinitiesForTesting(old_index, new_index, {});

    EncodedView old_view(old_index);
    EncodedView new_view(new_index);

    for (const auto& old_pool_tag_and_targets : old_index.target_pools()) {
      PoolTag pool_tag = old_pool_tag_and_targets.first;
      std::vector<uint32_t> old_labels;
      std::vector<uint32_t> new_labels;
      size_t label_bound = affinities[pool_tag.value()].AssignLabels(
          1.0, &old_labels, &new_labels);
      old_view.SetLabels(pool_tag, std::move(old_labels), label_bound);
      new_view.SetLabels(pool_tag, std::move(new_labels), label_bound);
    }

    std::vector<offset_t> old_sa =
        MakeSuffixArray<InducedSuffixSort>(old_view, old_view.Cardinality());

    EquivalenceMap equivalence_map;
    equivalence_map.Build(old_sa, old_view, new_view, affinities,
                          minimum_similarity);

    offset_t current_dst_offset = 0;
    offset_t coverage = 0;
    for (const auto& candidate : equivalence_map) {
      EXPECT_GE(candidate.eq.dst_offset, current_dst_offset);
      EXPECT_GT(candidate.eq.length, offset_t(0));
      EXPECT_LE(candidate.eq.src_offset + candidate.eq.length,
                old_index.size());
      EXPECT_LE(candidate.eq.dst_offset + candidate.eq.length,
                new_index.size());
      EXPECT_GE(candidate.similarity, minimum_similarity);
      current_dst_offset = candidate.eq.dst_offset;
      coverage += candidate.eq.length;
    }
    return coverage;
  };

  EXPECT_EQ(0U,
            test_build_equivalence(MakeImageIndexForTesting("", {}, {}),
                                   MakeImageIndexForTesting("", {}, {}), 4.0));

  EXPECT_EQ(0U, test_build_equivalence(
                    MakeImageIndexForTesting("", {}, {}),
                    MakeImageIndexForTesting("banana", {}, {}), 4.0));

  EXPECT_EQ(0U,
            test_build_equivalence(MakeImageIndexForTesting("banana", {}, {}),
                                   MakeImageIndexForTesting("", {}, {}), 4.0));

  EXPECT_EQ(0U, test_build_equivalence(
                    MakeImageIndexForTesting("banana", {}, {}),
                    MakeImageIndexForTesting("zzzz", {}, {}), 4.0));

  EXPECT_EQ(6U, test_build_equivalence(
                    MakeImageIndexForTesting("banana", {}, {}),
                    MakeImageIndexForTesting("banana", {}, {}), 4.0));

  EXPECT_EQ(6U, test_build_equivalence(
                    MakeImageIndexForTesting("bananaxx", {}, {}),
                    MakeImageIndexForTesting("bananayy", {}, {}), 4.0));

  EXPECT_EQ(8U, test_build_equivalence(
                    MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                    MakeImageIndexForTesting("banana11", {{6, 0}}, {}), 4.0));

  EXPECT_EQ(6U, test_build_equivalence(
                    MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                    MakeImageIndexForTesting("banana22", {}, {{6, 0}}), 4.0));

  EXPECT_EQ(
      15U,
      test_build_equivalence(
          MakeImageIndexForTesting("banana11pineapple", {{6, 0}}, {}),
          MakeImageIndexForTesting("banana22pineapple", {}, {{6, 0}}), 4.0));

  EXPECT_EQ(
      15U,
      test_build_equivalence(
          MakeImageIndexForTesting("bananaxxxxxxxxpineapple", {}, {}),
          MakeImageIndexForTesting("bananayyyyyyyypineapple", {}, {}), 4.0));

  EXPECT_EQ(
      19U,
      test_build_equivalence(
          MakeImageIndexForTesting("foobanana11xxpineapplexx", {{9, 0}}, {}),
          MakeImageIndexForTesting("banana11yypineappleyy", {{6, 0}}, {}),
          4.0));
}

TEST(EquivalenceMapTest, MakeForwardEquivalences) {
  EXPECT_EQ(std::vector<Equivalence>(),
            EquivalenceMap().MakeForwardEquivalences());
  EXPECT_EQ(std::vector<Equivalence>({{0, 0, 1}}),
            EquivalenceMap({{{0, 0, 1}, 0.0}}).MakeForwardEquivalences());
  EXPECT_EQ(std::vector<Equivalence>({{0, 0, 1}, {1, 1, 1}}),
            EquivalenceMap({{{0, 0, 1}, 0.0}, {{1, 1, 1}, 0.0}})
                .MakeForwardEquivalences());
  EXPECT_EQ(std::vector<Equivalence>({{0, 1, 1}, {1, 0, 1}}),
            EquivalenceMap({{{1, 0, 1}, 0.0}, {{0, 1, 1}, 0.0}})
                .MakeForwardEquivalences());
  EXPECT_EQ(
      std::vector<Equivalence>({{0, 2, 1}, {1, 0, 1}, {4, 1, 1}}),
      EquivalenceMap({{{1, 0, 1}, 0.0}, {{4, 1, 1}, 0.0}, {{0, 2, 1}, 0.0}})
          .MakeForwardEquivalences());
}

}  // namespace zucchini
