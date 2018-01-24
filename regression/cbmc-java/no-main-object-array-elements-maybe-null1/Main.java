public class Main
{
  public static class A {
    int x;
  }
  public A a;
  
  public static void main(Main[] args)
  {
    if(args != null && args.length > 0) {
      Main m = args[0];
      assert(m == null); // allowed to fail
    }
  }
}

