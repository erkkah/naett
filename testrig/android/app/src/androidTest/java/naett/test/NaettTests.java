package naett.test;

import org.junit.Test;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

public class NaettTests {
    @Test
    public void naett_Test() {
        try {
            System.loadLibrary("native-activity");
            assertTrue(runTests() != 0);
        } catch (Error e) {
            System.err.println(e.getMessage());
            fail();
        }
    }

    private native int runTests();
}
