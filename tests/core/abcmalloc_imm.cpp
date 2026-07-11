// [abcmalloc mirror] canonical umbrella first: cmalloc.hpp #defines
// MICRON_ABCMALLOC_DISABLE_STD so micron-core headers use THIS standalone
// allocator instead of pulling their own in-tree copy.
#include "../../src/cmalloc.hpp"
#include "../../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

#include <micron/string/strings.hpp>
#include <micron/vector/vector.hpp>

int
main()
{
  if constexpr ( true ) {
    mc::string first = "Hello World!";
    mc::string second = "!olleH dlroW";
    mc::vector<int> third(65536, 'A');      // shouldn't fire
    mc::posix::write(1, first.data(), first.size());
    mc::posix::write(1, second.data(), second.size());
    mc::vector<int> fourth(512, 'B');      // should fire
    mc::posix::write(1, "\n", 1);
    mc::posix::write(1, first.data(), 24);      // size got reset
    mc::posix::write(1, second.data(), 24);
    mc::string fifth = "reset once again";
    for ( int n : fourth ) mc::posix::write(1, &n, sizeof(int));
  }
  return 1;
}
