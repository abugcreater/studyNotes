# 59 | 模板模式（下）：模板模式与Callback回调函数有何区别和联系？
相对于普通的函数调用，回调是一种双向调用关系。A 类事先注册某个函数 F 到 B 类，A
类在调用 B 类的 P 函数的时候，B 类反过来调用 A 类注册给它的 F 函数。这里的 F 函数就
是“回调函数”。A 调用 B，B 反过来又调用 A，这种调用机制就叫作“回调”

实现:

```java
public interface ICallback {
	void methodToCallback();
}
public class BClass {
	public void process(ICallback callback) {
		//...
		callback.methodToCallback();
		//...
	}
}
public class AClass {
	public static void main(String[] args) {
		BClass b = new BClass();
		b.process(new ICallback() { //回调对象
		@Override
		public void methodToCallback() {
			System.out.println("Call back me.");
			}
		});
	}
}
```

应用举例:`JdbcTemplate`及`Runtime#addShutdownHook()`

回调可以细分为同步回调和异步回调。从应用场景上来看，同步回调看起来更像模板模式，异步回调看起来更像观察者模式。回调跟模板模式的区别，更多的是在代码实现上，而非应用场景上。回调基于组合关系来实现，模板模式基于继承关系来实现，回调比模板模式更加灵活。

灵活体现在:

- 像 Java 这种只支持单继承的语言，基于模板模式编写的子类，已经继承了一个父类，不再具有继承的能力
- 回调可以使用匿名类来创建回调对象，可以不用事先定义类；而模板模式针对不同的实现都要定义不同的子类。
- 如果某个类中定义了多个模板方法，每个方法都有对应的抽象方法，那即便我们只用到其中的一个模板方法，子类也必须实现所有的抽象方法。而回调就更加灵活，我们只需要往用到的模板方法中注入回调对象即可。