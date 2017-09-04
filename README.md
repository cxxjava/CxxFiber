# CxxFiber

## C++ fiber library

<br/>
*chinese version: [简体中文](README.zh_cn.md)*

### Table of Contents
  - [Characteristics](#characteristics)
  - [Example](#example)
  - [Performance](#performance)
  - [Dependency](#dependency)
  - [TODO](#todo)
  - [Support](#support)

#### Characteristics
* Cross platform: support Linux32/64, OSX64, support C++98;
* High performance: network performance is strong, support massive co process, since then no C1000K problem;
* Easy development: synchronous code, API elegant simplicity and efficient development;
* Mixed programming support process, threads and coroutines mixed use, carry out their duties: 

#### Example
  `c++:`
  
  ```
  #include "Eco.hh"
  
  int main(int argc, const char **argv) {
    // CxxJDK init.
    ESystem::init(argc, argv);
        
    try {
		EFiberScheduler scheduler;
		
		scheduler.schedule([&](){
			EServerSocket ss;
			ss.setReuseAddress(true);
			ss.bind(8888);

			while (!gStopFlag) {
				// accept
				sp<ESocket> socket = ss.accept();
				if (socket != null) {
					// create a new fiber
					scheduler.schedule([=](){
						try {
							char buf[512] = {0};
							// read
							EInputStream* is = socket->getInputStream();
							int n = is->read(buf, sizeof(buf));
							printf("read buf=%s\n", buf);

							// write
							EOutputStream* os = socket->getOutputStream();
							os->write(buf, n);
						} catch (EIOException& e) {
							e.printStackTrace();
						} catch (...) {
						}
					});
				}
			}
		});

		// begin schedule
		scheduler.join(); //signal thread mode
		//scheduler.join(4); //multi thread mode: 4 is count of threads
	}
	catch (EException& e) {
		e.printStackTrace();
	}
	catch (...) {
		printf("catch all...\n");
	}
    
    ESystem::exit(0);
    
    return 0;
  }
  
  ```

more examples:  
[testeco.cpp](test/testeco.cpp)  

#### Performance
`software environment:`

@see c++ example code: [benchmark.cpp](test/benchmark.cpp);


`hardware environment:`

```
Model Name:				MacBook Pro
Model Identifier:		MacBookPro10,2
Processor Speed:		2.6 GHz
Number of Processors:	1
Total Number of Cores:	2
```
`test results:`

![benchmark](img/benchmark.gif)

#### Dependency
`CxxFiber` is based on [CxxJDK](https://github.com/cxxjava/cxxjdk).  

#### TODO
    win64 support.

#### Support
Email: [cxxjava@163.com](mailto:cxxjava@163.com)

