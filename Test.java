public class Test {
  static {
    System.loadLibrary("test");
  }

  public static native Person persist(Person p);
  public static native Person load(Class<Person> cl);
  public static native void analyze(Person p);
  
  public static void pers() {
    Person p = new Person();
    p.age = 12;
    Person copy = persist(p);
    System.out.println(copy.age+" cl: "+copy.getClass());
    System.gc();
    System.out.println(copy.age);
  }
  
  public static void load() {
    Person copy = load(Person.class);
    System.out.println(copy.age+" cl: "+copy.getClass());
    System.gc();
    System.out.println(copy.age);
  }
  
  public static void analyze() {
    Person mom = new Person();
    mom.age = 52;
    
    Person p = new Person();
    p.age = 26;
    p.parents = new Person[]{ mom, mom, mom };
    analyze(p);
  }
  public static void main(String[] args) throws InterruptedException {
    //Thread.sleep(20000);
    //pers();
    //load();
    analyze();
  }
}
