# std-like-thread
スレッド属性を指定可能なstd風のthreadクラス。<br>
標準のstd::threadは便利だけど、スタックサイズやアフィニティマスクを指定したい、そんな人向けです。(要c++14)

```cpp
slt::thread::attributes attrs;
attrs.stackSize = 512 * 1024;
attrs.priority = slt::thread::priority::normal;
attrs.affinity = slt::thread::affinity::user0 | slt::thread::affinity::user1;
attrs.name = "thread name for debugging";
auto thr = slt::thread(attrs, [] (const char* str) { std::printf(str); }, "hello, world");
thr.join();
```
