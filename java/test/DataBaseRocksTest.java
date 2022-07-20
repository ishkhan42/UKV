
import com.unum.ukv.DataBaseRocks;
import org.junit.Test;

import java.util.Arrays;

public class DataBaseRocksTest {
    static {
        DataBaseRocks.init();
    }
    @Test
    public void test() {
        DataBaseRocks.Context ctx = new DataBaseRocks.Context("");
        ctx.put(42, "hey".getBytes());
        assert Arrays.equals(ctx.get(42), "hey".getBytes()) : "Received wrong value";

        DataBaseRocks.Transaction txn = ctx.transaction();
        txn.put("any", 42, "meaning of life".getBytes());
        assert Arrays.equals(txn.get("any", 42), "meaning of life".getBytes()) : "Wrong philosophy";
        txn.commit();
        assert Arrays.equals(ctx.get("any", 42), "meaning of life".getBytes()) : "Accepted wrong philosophy";

        ctx.close();
        System.out.println("Success!");
    }
}
