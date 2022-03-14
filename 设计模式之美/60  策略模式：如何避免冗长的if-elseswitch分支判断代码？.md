# 60 | 策略模式：如何避免冗长的if-else/switch分支判断代码？

策略模式(Strategy Design Pattern):定义一族算法类，将每个算法分别封装起来，让它们可以互相替换。策略模式可以使算法的变化独立于使用它们的客户端（这里的客户端代指使用算法的代码）

完整的策略模式包括:策略的定义,策略的创建和策略的使用

- 定义指定义一个策略接口和一组实现这个接口的实现类.
- 创建指创建策略的逻辑抽象出来放在一个工厂类中,可以提前创建完缓存起来,也可以实时创建新的策略类.
- 使用一般运行时动态确定使用哪种策略是最典型的场景,非动态确定并不能发挥策略模式的优势.



除此之外，我们还可以通过策略模式来移除 if-else 分支判断。实际上，这得益于策略工厂类，更本质上点讲，是借助“查表法”，根据 type 查表替代根据 type 分支判断。





demo根据文件大小利用策略模式进行排序

```java
public class Sorter {
  private static final long GB = 1000 * 1000 * 1000;
  private static final List<AlgRange> algs = new ArrayList<>();
  static {
    algs.add(new AlgRange(0, 6*GB, SortAlgFactory.getSortAlg("QuickSort")));
    algs.add(new AlgRange(6*GB, 10*GB, SortAlgFactory.getSortAlg("ExternalSort")));
    algs.add(new AlgRange(10*GB, 100*GB, SortAlgFactory.getSortAlg("ConcurrentExternalSort")));
    algs.add(new AlgRange(100*GB, Long.MAX_VALUE, SortAlgFactory.getSortAlg("MapReduceSort")));
  }
  public void sortFile(String filePath) {
    // 省略校验逻辑
    File file = new File(filePath);
    long fileSize = file.length();
    ISortAlg sortAlg = null;
    for (AlgRange algRange : algs) {
      if (algRange.inRange(fileSize)) {
        sortAlg = algRange.getAlg();
        break;
      }
    }
    sortAlg.sort(filePath);
  }
  private static class AlgRange {
    private long start;
    private long end;
    private ISortAlg alg;
    public AlgRange(long start, long end, ISortAlg alg) {
      this.start = start;
      this.end = end;
      this.alg = alg;
    }
    public ISortAlg getAlg() {
      return alg;
    }
    public boolean inRange(long size) {
      return size >= start && size < end;
    }
  }
}

```

