# Faiss测试套件

这是一个[Faiss](https://github.com/facebookresearch/faiss)的测试套件，提供了四个通用工具（subset, index, groundtruth和benchmark）以及一个针对组测试脚本（scripts/)。

## subset

该工具用来从一个大数据集中提取一个小数据集。使用方法为：
```
./subset <src> <dst> <n>
```
其中，src就是大数据集的文件，dst就是生成的小数据集文件，n是提取的条数。该工具会从src中随机挑选n条，因此每次产生的dst是不同的。src和dst都可以是bvecs、ivecs、fvecs以及它们的gz压缩包。subset会自动处理解压和压缩工作，以及数据类型转换工作。

使用示例：
```
./subset bigann.bvecs.gz small.fvecs 1000
./subset sift1M_base.fvecs sift500K.fvecs.gz 500000
```

## randset

该工具用来生成一个随机数据集。使用方法为：
```
./randset <dst> <dim> <n> <min> <max>
```
其中，dst是输出文件路径，dim是向量维度，n是向量的个数。min和max是向量每一个维度的取值范围。dst可以是bvecs、ivecs、fvecs以及它们的gz压缩包。randset会自动处理解压和压缩工作，以及数据类型转换工作。

使用示例：
```
./randset rand10M.fvecs 128 10000000 -100.0 123.4
```

注意，经验而谈，算法运行在随机数据集（比如rand1M）的性能可以代表算法运行在有意义的数据集（比如sift1M）上的性能，但是召回率会比有意义的数据集差很多。因此，randset的适用场合是测试算法在不同规模数据集上的性能特性。其召回率一般不具有参考意义。

## index

该工具专门处理index，包括两方面：
1) 构建index;
2) 估算index占用内存大小。

当用于构建index时，使用方法为：
```
./index build <fpath> <key> <parameters> <base> <train_ratio>
```
其中fpath是构建后的index的存储路径，key为index的类型（比如"IVF1024,PQ64"，格式与faiss::index_factory()相同），parameters为需要传给index的参数（比如"verbose=1,nprobe=10"，格式与faiss::ParameterSpace相同），base是整个数据集的文件路径，train_ratio是一个0～1之间的小数，表示从base中抽取多少数据作为训练数据集。与subset一样，base可以是bvecs、ivecs、fvecss以及它们的gz压缩包，index会自动处理解压和压缩工作，以及数据类型转换工作。

使用示例：
```
./index build myindex.idx IVF1024,Flat verbose=0 sift1M_base.fvecs 0.1
```

当用于估算index占用内存大小时，使用方法为：
```
./index size <fpath>
```
其中fpath是index的路径。该命令输出一个数值，为index占用的内存大小，以MB计。

## groundtruth

该工具用于计算groundtruth。使用方法为：
```
./groundtruth <gt> <base> <query> <metric> <top_n> <thread>
```
其中，gt是产生的groundtruth的存储路径，base是整个数据集的路径，query是查询数据集的路径，metric是距离计算方法（目前支持"l1"和"l2"，即曼哈顿距离与欧式距离），top_n指定最近邻的个数，thread是使用多少个线程并行加速（不影响最终结果，只影响速度）。base和query可以是bvecs、ivecs、fvecss以及它们的gz压缩包，但是gt必须是ivecs或者ivecs.gz。

使用示例：
```
./groundtruth sift1M_gt_1K.ivecs sift1M_base.fvecs sift1M_query.fvecs l2 1000 4
```

## benchmark

以上4个工具都是辅助的，benchmark才是核心。使用方法为：
```
./benchmark <index> <query> <gt> <top_n> <percentages> <cases>
```
其中，index是index的存储路径，query是查询数据集的路径，gt是groundtruth的存储路径，top_n是最近邻的个数，percentages是以逗号分隔的若干个百分位数，cases是以分号分隔的若干个测试用例。一样的，query可以是bvecs、ivecs、fvecss以及它们的gz压缩包，gt必须是ivecs或者ivecs.gz。

top_n的取值只要不超过gt中的top_n即可。比如使用groundtruth产生sift1M_gt_1K.ivecs时，传入的top_n参数是1000，意味这sift1M_gt_1K.ivecs中包含了sift1M_query.fvecs中每一条向量的1000个最近邻。那么把sift1M_gt_1K.ivecs作为gt参数传给benchmark工具时，top_n只要不超过1000都可以，benchmark会自动截取指定的top_n个最近邻。

benchmark会为每一个测试用例输出测试结果，格式如下：
```
qps: 887.449
cpu-util: 4.10067
mem-r-bw: 7220.37
mem-w-bw: 16.0404
latency: best=3269 worst=7687 average=4506.26 P(50%)=4499 P(99%)=5599 P(99.9%)=5881
recall: best=1 worst=0.71 average=0.902705 P(50%)=0.9 P(99%)=0.81 P(99.9%)=0.77
```
分别为qps（即每秒请求数），cpu利用率（比如上面的4.10067就相当与top命令中显示410.1%，即平均动用了4.1个处理器核心），内存读带宽（MB/s），内存写带宽（MB/s），请求延迟统计（毫秒）和召回率统计。统计信息包括了最好情况、最差情况和平均值，附加若干个用户指定的百分位数。

percentages即用户指定的百分位数，如果用户传入"50,99,99.9"就会得到如同上面的统计。

cases是若干个测试用例。一次benchmark命令可以执行多个测试用例，这样可以避免重复的准备工作（比如加载index、query和groundtruth），从而大幅提高效率。单个测试用例的的语法为：
```
<parameters>/<batch_size>x<thread_count>[:<cpu_list>]
```
其中parameters是一个用逗号分隔的参数列表（格式与faiss::ParameterSpace相同），用于配置index。比如"nprobe=64/1x8"的含义即为，把index的nprobe设置为64，然后使用8线程、batch大小为1的方式执行测试。case可以加上可选项cpu_list，表明各个线程分别绑定在哪些核心上。而case之间使用分号分隔以构成cases。

使用示例：
```
./benchmark myidex.idx sift1M_query.fvecs sift1M_gt_1K.ivecs 100 50,99,99.9 'nprobe=64/1x4;nprobe=128/1x8;nprobe=32,verbose=1/8x2:0,1'
```
注意，使用shell时，用于shell会把分号看作命令参数的分隔符，因此我们需要用引号将cases包起来，以避免shell的“过度解读”。

## 依赖

1) zlib，大多数linux都自带了;
2) faiss, 可以`git clone https://github.com/facebookresearch/faiss.git`;
3) pcm（用于获取内存带宽等硬件信息）, 可以`git clone https://github.com/opcm/pcm.git`;

修改Makefile中的FAISS_DIR和PCM_DIR，之后`make`即可得到以上四个可执行文件。运行index和benchmark时，需要动态加载libfaiss.so，因此需要设置好LD_LIBRARY_PATH。

另外，运行benchmark时，会访问MSR，这个需要首先`sudo modprobe msr`加载msr内核模块，然后以root权限运行benchmark。

## 测试脚本

该目录包含了benchmark.py和config_env.py，config_env.py用于配置测试的方法，包括libfaiss.so所在目录、数据所在目录、测试线程数、批处理组大小、top-n等信息。benchmark.py利用config_env.py中的配置进行测试。此外还有report_ivfflat.py和report_ivfpq.py等专用于某种算法的测试脚本。换言之，benchmark.py和config_env.py实现了算法无关的测试框架，而不同的算法只需要使用不同的report_*.py来调用benchmark.py即可。这样的设计也非常有利于扩展。