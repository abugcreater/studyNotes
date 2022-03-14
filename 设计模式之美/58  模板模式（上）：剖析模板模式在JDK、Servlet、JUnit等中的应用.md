# 58 | 模板模式（上）：剖析模板模式在JDK、Servlet、JUnit等中的应用

模版模式(Template Method Design Pattern):模板方法模式在一个方法中定义一个算法骨架，并将某些步骤推迟到子类中实现。模板方法模式可以让子类在不改变算法整体结构的情况下，重新定义算法中的某些步骤。

实现:

```
public abstract class AbstractClass {
	public final void templateMethod() {
		//...
		method1();
		//...
		method2();
		//...
	}
	protected abstract void method1();
	protected abstract void method2();
}
如以上代码所示,父类负责定义整个算法逻辑,子类定义具体实现.
```

模版模式的作用一:复用(所有子类可以复用父类中提供的模版方法的代码)

如`Java InputStream`中`read()`方法,还有`AbstractList`中`readall`和`read`的关系

作用二:扩展（指框架的扩展性,让框架用于可以在不修改框架源码的情况下,基于扩展点定制化框架的功能)

如`HttpServlet`中`service`方法定义了运算流程,子类实现具体执行的步骤

```
protected void service(HttpServletRequest req, HttpServletResponse resp)
    throws ServletException, IOException
{
    String method = req.getMethod();

    if (method.equals(METHOD_GET)) {
        long lastModified = getLastModified(req);
        if (lastModified == -1) {
            // servlet doesn't support if-modified-since, no reason
            // to go through further expensive logic
            doGet(req, resp);
        } else {
            long ifModifiedSince = req.getDateHeader(HEADER_IFMODSINCE);
            if (ifModifiedSince < lastModified) {
                // If the servlet mod time is later, call doGet()
                // Round down to the nearest second for a proper compare
                // A ifModifiedSince of -1 will always be less
                maybeSetLastModified(resp, lastModified);
                doGet(req, resp);
            } else {
                resp.setStatus(HttpServletResponse.SC_NOT_MODIFIED);
            }
        }

    } else if (method.equals(METHOD_HEAD)) {
        long lastModified = getLastModified(req);
        maybeSetLastModified(resp, lastModified);
        doHead(req, resp);

    } else if (method.equals(METHOD_POST)) {
        doPost(req, resp);
        
    } else if (method.equals(METHOD_PUT)) {
        doPut(req, resp);
        
    } else if (method.equals(METHOD_DELETE)) {
        doDelete(req, resp);
        
    } else if (method.equals(METHOD_OPTIONS)) {
        doOptions(req,resp);
        
    } else if (method.equals(METHOD_TRACE)) {
        doTrace(req,resp);
        
    } else {
        //
        // Note that this means NO servlet supports whatever
        // method was requested, anywhere on this server.
        //

        String errMsg = lStrings.getString("http.method_not_implemented");
        Object[] errArgs = new Object[1];
        errArgs[0] = method;
        errMsg = MessageFormat.format(errMsg, errArgs);
        
        resp.sendError(HttpServletResponse.SC_NOT_IMPLEMENTED, errMsg);
    }
}
```

另外还有`TestCase#runBare`方法



在模板模式经典的实现中，模板方法定义为 final，可以避免被子类重写。需要子类重写的
方法定义为 abstract，可以强迫子类去实现。不过，在实际项目开发中，模板模式的实现
比较灵活，以上两点都不是必须的。