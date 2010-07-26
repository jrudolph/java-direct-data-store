public class Test {
  static {
    System.loadLibrary("test");
  }

  public static native Person get();
  public static native Person persist(Person p);
  
  public static void main(String[] args) throws InterruptedException {
    //Thread.sleep(10000);
    Person p = new Person();
    p.age = 28;
    Person copy = persist(p);
    System.out.println(copy.age);
    System.gc();
    System.out.println(copy.age);
  }
}
