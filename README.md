# std-like thread
スレッド属性を指定可能なstd::thread風のクラス。  
std::thread には、スレッドのプライオリティ変更などを行うために native_handle() が提供されています。  
しかし native_handle() を使用した操作にはいくつかの問題があります。

1. ハンドル型に対する操作は環境依存のプログラミングが必要になる。
1. スレッドが起動してから設定するので、指定できない属性がある。

そのため、スレッドを起動する前に必要な属性を指定できる必要があります。
```cpp
// 通常の属性でスレッドを実行
auto thr = th::thread([] (const char* str) { std::printf(str); }, "hello, world");
thr.join();

// 属性を指定してスレッドを実行
th::thread::attributes attrs;
attrs.stackSize = 512 * 1024;
attrs.priority = th::thread::PRIORITY_NORMAL;
attrs.affinity = 0x03;
attrs.name = "thread name for debugging";
thr = th::thread(attrs, [] (const char* str) { std::printf(str); }, "hello, world");
thr.join();
```
