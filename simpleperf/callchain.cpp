/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "callchain.h"

#include <queue>
#include <base/logging.h>
#include "sample_tree.h"

static bool MatchSample(const SampleEntry* sample1, const SampleEntry* sample2) {
  return (sample1->symbol->name == sample2->symbol->name);
}

static size_t MatchSamples(const std::vector<SampleEntry*>& samples1,
                           const std::vector<SampleEntry*>& samples2, size_t samples2_start) {
  size_t i, j;
  for (i = 0, j = samples2_start; i < samples1.size() && j < samples2.size(); ++i, ++j) {
    if (!MatchSample(samples1[i], samples2[j])) {
      break;
    }
  }
  return i;
}

static CallChainNode* SelectMatchingNode(const std::vector<std::unique_ptr<CallChainNode>>& nodes,
                                         const SampleEntry* sample) {
  for (auto& node : nodes) {
    if (MatchSample(node->chain.front(), sample)) {
      return node.get();
    }
  }
  return nullptr;
}

static std::unique_ptr<CallChainNode> AllocateNode(const std::vector<SampleEntry*>& chain,
                                                   size_t chain_start, uint64_t period,
                                                   uint64_t children_period) {
  std::unique_ptr<CallChainNode> node(new CallChainNode);
  for (size_t i = chain_start; i < chain.size(); ++i) {
    node->chain.push_back(chain[i]);
  }
  node->period = period;
  node->children_period = children_period;
  return node;
}

static void SplitNode(CallChainNode* parent, size_t parent_length) {
  std::unique_ptr<CallChainNode> child =
      AllocateNode(parent->chain, parent_length, parent->period, parent->children_period);
  child->children = std::move(parent->children);
  parent->period = 0;
  parent->children_period = child->period + child->children_period;
  parent->chain.resize(parent_length);
  parent->children.clear();
  parent->children.push_back(std::move(child));
}

void CallChainRoot::AddCallChain(const std::vector<SampleEntry*>& callchain, uint64_t period) {
  children_period += period;
  CallChainNode* p = SelectMatchingNode(children, callchain[0]);
  if (p == nullptr) {
    std::unique_ptr<CallChainNode> new_node = AllocateNode(callchain, 0, period, 0);
    children.push_back(std::move(new_node));
    return;
  }
  size_t callchain_pos = 0;
  while (true) {
    size_t match_count = MatchSamples(p->chain, callchain, callchain_pos);
    CHECK_GT(match_count, 0u);
    callchain_pos += match_count;
    bool find_child = true;
    if (match_count < p->chain.size()) {
      SplitNode(p, match_count);
      find_child = false;  // No need to find matching node in p->children.
    }
    if (callchain_pos == callchain.size()) {
      p->period += period;
      return;
    }
    p->children_period += period;
    if (find_child) {
      CallChainNode* np = SelectMatchingNode(p->children, callchain[callchain_pos]);
      if (np != nullptr) {
        p = np;
        continue;
      }
    }
    std::unique_ptr<CallChainNode> new_node = AllocateNode(callchain, callchain_pos, period, 0);
    p->children.push_back(std::move(new_node));
    break;
  }
}

static bool CompareNodeByPeriod(const std::unique_ptr<CallChainNode>& n1,
                                const std::unique_ptr<CallChainNode>& n2) {
  uint64_t period1 = n1->period + n1->children_period;
  uint64_t period2 = n2->period + n2->children_period;
  return period1 > period2;
}

void CallChainRoot::SortByPeriod() {
  std::queue<std::vector<std::unique_ptr<CallChainNode>>*> queue;
  queue.push(&children);
  while (!queue.empty()) {
    std::vector<std::unique_ptr<CallChainNode>>* v = queue.front();
    queue.pop();
    std::sort(v->begin(), v->end(), CompareNodeByPeriod);
    for (auto& node : *v) {
      queue.push(&node->children);
    }
  }
}
