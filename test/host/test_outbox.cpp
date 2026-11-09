#include "core/outbox.h"
#include "mini_test.h"

TEST(classify) {
  CHECK(classifyHttp(200) == SendOutcome::Sent);
  CHECK(classifyHttp(201) == SendOutcome::Sent);
  CHECK(classifyHttp(-1) == SendOutcome::Retry);
  CHECK(classifyHttp(0) == SendOutcome::Retry);
  CHECK(classifyHttp(500) == SendOutcome::Retry);
  CHECK(classifyHttp(503) == SendOutcome::Retry);
  CHECK(classifyHttp(408) == SendOutcome::Retry);
  CHECK(classifyHttp(429) == SendOutcome::Retry);
  CHECK(classifyHttp(400) == SendOutcome::Rejected);
  CHECK(classifyHttp(401) == SendOutcome::Rejected);
  CHECK(classifyHttp(404) == SendOutcome::Rejected);
}

TEST(outbox_backoff) {
  OutboxScheduler o;
  std::vector<std::string> p{"a", "b"};
  CHECK_EQ(o.next(p, 0), "a");
  CHECK(o.inFlight());
  CHECK_EQ(o.next(p, 0), "");  // one in flight at a time
  o.onResult("a", SendOutcome::Retry, 0);
  CHECK(!o.inFlight());
  CHECK_EQ(o.next(p, 100), "b");  // a is backing off, b goes
  o.onResult("b", SendOutcome::Sent, 100);
  p = {"a"};
  CHECK_EQ(o.next(p, 4999), "");
  CHECK_EQ(o.next(p, 5000), "a");
  o.onResult("a", SendOutcome::Retry, 5000);
  CHECK_EQ(o.next(p, 14999), "");
  o.kick();
  CHECK_EQ(o.next(p, 14999), "a");
}

TEST(outbox_ignores_stale_result) {
  OutboxScheduler o;
  std::vector<std::string> p{"a"};
  CHECK_EQ(o.next(p, 0), "a");
  o.onResult("zzz", SendOutcome::Sent, 0);  // not the in-flight id
  CHECK(o.inFlight());
}

TEST(outbox_releases_lost_request) {
  OutboxScheduler o;
  std::vector<std::string> p{"a"};
  CHECK_EQ(o.next(p, 0), "a");
  CHECK_EQ(o.next(p, 29999), "");
  CHECK_EQ(o.next(p, 30000), "");  // released and backing off as a retry
  CHECK(!o.inFlight());
  CHECK_EQ(o.next(p, 35000), "a");
}

TEST(retry_delay_cap) {
  CHECK_EQ(retryDelayMs(1), 5000u);
  CHECK_EQ(retryDelayMs(2), 10000u);
  CHECK_EQ(retryDelayMs(4), 40000u);
  CHECK_EQ(retryDelayMs(5), 60000u);
  CHECK_EQ(retryDelayMs(9), 60000u);
  CHECK_EQ(retryDelayMs(1000), 60000u);
}
