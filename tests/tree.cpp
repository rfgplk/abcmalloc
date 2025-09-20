#include "../src/cmalloc.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

constexpr int MAX_CHILDREN = 10;
constexpr size_t MAX_DATA_SIZE = 4096;
constexpr int NUM_TEST_NODES = 100000;
constexpr int VERIFICATION_PASSES = 3;

class tree_node_t;

class test_stats_t
{
public:
  int nodes_created = 0;
  int nodes_verified = 0;
  int data_corruptions = 0;
  int malloc_failures = 0;
  size_t total_memory_allocated = 0;

  void
  print() const
  {
    std::cout << "\n=== Test Statistics ===\n";
    std::cout << "Nodes created: " << nodes_created << "\n";
    std::cout << "Nodes verified: " << nodes_verified << "\n";
    std::cout << "Data corruptions: " << data_corruptions << "\n";
    std::cout << "Malloc failures: " << malloc_failures << "\n";
    std::cout << "Total memory allocated: " << total_memory_allocated << " bytes\n";
    if ( nodes_created > 0 ) {
      std::cout << "Average data per node: " << std::fixed << std::setprecision(2)
                << static_cast<double>(total_memory_allocated) / nodes_created << " bytes\n";
    }
  }
};

class tree_node_t
{
private:
  int id_;
  char *data_;
  size_t data_size_;
  tree_node_t **children_;
  int num_children_;
  int max_children_;
  tree_node_t *parent_;

public:
  tree_node_t(int id, const char *data, size_t data_size, test_stats_t &stats)
      : id_(id), data_size_(data_size), num_children_(0), max_children_(MAX_CHILDREN), parent_(nullptr)
  {

    void *raw_data = abc::malloc(data_size + 1);
    if ( !raw_data ) {
      stats.malloc_failures++;
      throw std::bad_alloc();
    }
    data_ = static_cast<char *>(raw_data);
    std::memcpy(data_, data, data_size);
    data_[data_size] = '\0';

    void *raw_children = abc::malloc(sizeof(tree_node_t *) * max_children_);
    if ( !raw_children ) {
      stats.malloc_failures++;
      abc::free(data_);
      throw std::bad_alloc();
    }
    children_ = static_cast<tree_node_t **>(raw_children);

    for ( int i = 0; i < max_children_; ++i ) {
      children_[i] = nullptr;
    }

    stats.nodes_created++;
    stats.total_memory_allocated += sizeof(tree_node_t) + data_size + 1 + (sizeof(tree_node_t *) * max_children_);
  }

  ~tree_node_t()
  {
    abc::free(data_);
    abc::free(children_);
  }

  tree_node_t(const tree_node_t &) = delete;
  tree_node_t &operator=(const tree_node_t &) = delete;

  tree_node_t(tree_node_t &&other) noexcept
      : id_(other.id_), data_(other.data_), data_size_(other.data_size_), children_(other.children_),
        num_children_(other.num_children_), max_children_(other.max_children_), parent_(other.parent_)
  {

    other.data_ = nullptr;
    other.children_ = nullptr;
    other.num_children_ = 0;
    other.parent_ = nullptr;
  }

  tree_node_t &
  operator=(tree_node_t &&other) noexcept
  {
    if ( this != &other ) {
      abc::free(data_);
      abc::free(children_);

      id_ = other.id_;
      data_ = other.data_;
      data_size_ = other.data_size_;
      children_ = other.children_;
      num_children_ = other.num_children_;
      max_children_ = other.max_children_;
      parent_ = other.parent_;

      other.data_ = nullptr;
      other.children_ = nullptr;
      other.num_children_ = 0;
      other.parent_ = nullptr;
    }
    return *this;
  }

  int
  getId() const
  {
    return id_;
  }
  const char *
  getData() const
  {
    return data_;
  }
  size_t
  getDataSize() const
  {
    return data_size_;
  }
  int
  getNumChildren() const
  {
    return num_children_;
  }
  tree_node_t *
  getChild(int index) const
  {
    return (index >= 0 && index < num_children_) ? children_[index] : nullptr;
  }
  tree_node_t *
  getParent() const
  {
    return parent_;
  }

  bool
  addChild(tree_node_t *child)
  {
    if ( !child || num_children_ >= max_children_ ) {
      return false;
    }

    children_[num_children_] = child;
    child->parent_ = this;
    num_children_++;
    return true;
  }

  bool
  verifyData(int expected_id, const char *expected_data, test_stats_t &stats) const
  {
    stats.nodes_verified++;

    if ( id_ != expected_id ) {
      std::cout << "ERROR: Node ID mismatch. Expected: " << expected_id << ", Got: " << id_ << "\n";
      stats.data_corruptions++;
      return false;
    }

    if ( std::strcmp(data_, expected_data) != 0 ) {
      std::cout << "ERROR: Node data corruption detected!\n";
      std::cout << "  Expected: " << std::string(expected_data).substr(0, 50);
      if ( std::strlen(expected_data) > 50 )
        std::cout << "...";
      std::cout << "\n  Got:      " << std::string(data_).substr(0, 50);
      if ( std::strlen(data_) > 50 )
        std::cout << "...";
      std::cout << "\n";
      stats.data_corruptions++;
      return false;
    }

    return true;
  }

  void
  verifyTreeIntegrity(test_stats_t &stats) const
  {
    for ( int i = 0; i < num_children_; ++i ) {
      if ( children_[i]->parent_ != this ) {
        std::cout << "ERROR: Parent-child relationship corruption!\n";
        stats.data_corruptions++;
      }
      children_[i]->verifyTreeIntegrity(stats);
    }
  }

  void
  printTree(int depth = 0) const
  {
    if ( depth > 3 )
      return;

    for ( int i = 0; i < depth; ++i ) {
      std::cout << "  ";
    }

    std::string data_preview(data_);
    if ( data_preview.length() > 30 ) {
      data_preview = data_preview.substr(0, 30) + "...";
    }

    std::cout << "Node " << id_ << ": " << data_preview << " (" << num_children_ << " children)\n";

    for ( int i = 0; i < num_children_; ++i ) {
      children_[i]->printTree(depth + 1);
    }
  }
};

void
destroy_tree(tree_node_t *root)
{
  if ( !root )
    return;

  for ( int i = 0; i < root->getNumChildren(); ++i ) {
    destroy_tree(root->getChild(i));
  }

  delete root;
}

class random_generator_t
{
private:
  std::mt19937 gen_;
  std::uniform_int_distribution<> char_dist_;
  std::uniform_int_distribution<size_t> size_dist_;

  static constexpr char charset_[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

public:
  random_generator_t()
      : gen_(std::chrono::steady_clock::now().time_since_epoch().count()), char_dist_(0, sizeof(charset_) - 2),
        size_dist_(10, MAX_DATA_SIZE)
  {
  }

  std::string
  random_string(size_t size)
  {
    std::string result;
    result.reserve(size);

    for ( size_t i = 0; i < size; ++i ) {
      result += charset_[char_dist_(gen_)];
    }

    return result;
  }

  std::string
  random_string()
  {
    return random_string(size_dist_(gen_));
  }

  int
  random_int(int max)
  {
    std::uniform_int_distribution<> dist(0, max - 1);
    return dist(gen_);
  }
};

constexpr char random_generator_t::charset_[];

class malloc_test
{
private:
  test_stats_t stats_;
  random_generator_t rng_;

public:
  void
  run()
  {
    std::cout << "Starting N-Tree Malloc Test...\n";
    std::cout << "Creating " << NUM_TEST_NODES << " nodes with random data\n\n";

    std::vector<tree_node_t *> all_nodes;
    std::vector<std::string> expected_data;

    all_nodes.reserve(NUM_TEST_NODES);
    expected_data.reserve(NUM_TEST_NODES);

    tree_node_t *root = nullptr;

    try {
      std::string root_data = rng_.random_string(50);
      root = new tree_node_t(0, root_data.c_str(), root_data.length(), stats_);

      all_nodes.push_back(root);
      expected_data.push_back(root_data);

      for ( int i = 1; i < NUM_TEST_NODES; ++i ) {
        std::string data = rng_.random_string();

        tree_node_t *node = new tree_node_t(i, data.c_str(), data.length(), stats_);
        all_nodes.push_back(node);
        expected_data.push_back(data);

        int parent_idx = rng_.random_int(i);
        if ( !all_nodes[parent_idx]->addChild(node) ) {
          root->addChild(node);
        }

        if ( i % 100 == 0 ) {
          std::cout << "Created " << i << " nodes...\n";
        }
      }

      std::cout << "Tree creation complete!\n\n";

      for ( int pass = 0; pass < VERIFICATION_PASSES; ++pass ) {
        std::cout << "=== Verification Pass " << (pass + 1) << " ===\n";

        int corruptions_before = stats_.data_corruptions;

        for ( size_t i = 0; i < all_nodes.size(); ++i ) {
          all_nodes[i]->verifyData(static_cast<int>(i), expected_data[i].c_str(), stats_);
        }

        root->verifyTreeIntegrity(stats_);

        std::cout << "Pass " << (pass + 1)
                  << " complete. New corruptions: " << (stats_.data_corruptions - corruptions_before) << "\n";

        for ( volatile int j = 0; j < 1000000; ++j ) {
        }
      }

      std::cout << "\n=== Tree Structure Sample ===\n";
      root->printTree();

    } catch ( const std::exception &e ) {
      std::cout << "Exception during test: " << e.what() << "\n";
    }

    destroy_tree(root);

    std::cout << "\n=== Test Complete ===\n";
    stats_.print();

    if ( stats_.data_corruptions == 0 && stats_.malloc_failures == 0 ) {
      std::cout << "\n✅ SUCCESS: All tests passed! Memory allocation and data integrity verified.\n";
    } else {
      std::cout << "\n❌ FAILURE: Detected " << stats_.data_corruptions << " data corruptions and "
                << stats_.malloc_failures << " malloc failures.\n";
    }
  }
};

int
main()
{
  std::cout << "N-Tree Malloc Test Program (C++)\n";
  std::cout << "=================================\n\n";

  malloc_test test;
  test.run();

  return 0;
}
