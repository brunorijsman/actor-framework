// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE intrusive_ptr

#include "caf/intrusive_ptr.hpp"

#include "core-test.hpp"

// this test doesn't verify thread-safety of intrusive_ptr
// however, it is thread safe since it uses atomic operations only

#include <cstddef>
#include <vector>

#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"

using namespace caf;

namespace {

int class0_instances = 0;
int class1_instances = 0;

class class0;
class class1;

using class0ptr = intrusive_ptr<class0>;
using class1ptr = intrusive_ptr<class1>;

class class0 : public ref_counted {
public:
  explicit class0(bool subtype = false) : subtype_(subtype) {
    if (!subtype) {
      ++class0_instances;
    }
  }

  ~class0() override {
    if (!subtype_) {
      --class0_instances;
    }
  }

  bool is_subtype() const {
    return subtype_;
  }

  virtual class0ptr create() const {
    return make_counted<class0>();
  }

private:
  bool subtype_;
};

class class1 : public class0 {
public:
  class1() : class0(true) {
    ++class1_instances;
  }

  ~class1() override {
    --class1_instances;
  }

  class0ptr create() const override {
    return make_counted<class1>();
  }
};

class0ptr get_test_rc() {
  return make_counted<class0>();
}

class0ptr get_test_ptr() {
  return get_test_rc();
}

struct fixture {
  ~fixture() {
    CHECK_EQ(class0_instances, 0);
    CHECK_EQ(class1_instances, 0);
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(make_counted) {
  auto p = make_counted<class0>();
  CHECK_EQ(class0_instances, 1);
  CHECK(p->unique());
}

CAF_TEST(reset) {
  class0ptr p;
  p.reset(new class0, false);
  CHECK_EQ(class0_instances, 1);
  CHECK(p->unique());
}

CAF_TEST(get_test_rc) {
  class0ptr p1;
  p1 = get_test_rc();
  class0ptr p2 = p1;
  CHECK_EQ(class0_instances, 1);
  CHECK_EQ(p1->unique(), false);
}

CAF_TEST(list) {
  std::vector<class0ptr> pl;
  pl.push_back(get_test_ptr());
  pl.push_back(get_test_rc());
  pl.push_back(pl.front()->create());
  CHECK(pl.front()->unique());
  CHECK_EQ(class0_instances, 3);
}

CAF_TEST(full_test) {
  auto p1 = make_counted<class0>();
  CHECK_EQ(p1->is_subtype(), false);
  CHECK_EQ(p1->unique(), true);
  CHECK_EQ(class0_instances, 1);
  CHECK_EQ(class1_instances, 0);
  p1.reset(new class1, false);
  CHECK_EQ(p1->is_subtype(), true);
  CHECK_EQ(p1->unique(), true);
  CHECK_EQ(class0_instances, 0);
  CHECK_EQ(class1_instances, 1);
  auto p2 = make_counted<class1>();
  p1 = p2;
  CHECK_EQ(p1->unique(), false);
  CHECK_EQ(class0_instances, 0);
  CHECK_EQ(class1_instances, 1);
  CHECK_EQ(p1, static_cast<class0*>(p2.get()));
}

END_FIXTURE_SCOPE()
