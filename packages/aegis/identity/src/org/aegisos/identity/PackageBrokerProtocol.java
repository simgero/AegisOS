package org.aegisos.identity;

import android.util.JsonReader;
import android.util.JsonToken;
import java.io.IOException;
import java.io.StringReader;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/** Bounded metadata protocol to the authenticated native owner. Never an approval grant. */
final class PackageBrokerProtocol {
    static final int BEGIN=8, PLAN=9, REVIEW=10, PREPARE=11, STATUS=12, START=13, CANCEL=14;
    static final int MAX_REPLY=65536, PREFIX=48;
    static final int INSTALL=1, UPDATE=2, REMOVE=3, PERSONAL=1, SHARED=2;
    static final int SELECTING=0, SELECTED=1, PLANNING=2, COLLECTED=3, REVIEWED=4,
            PREPARING=5, PREPARED=6, RUNNING=7, VALIDATING=8, PUBLISHING=9, COMPLETE=10, SEALED=11;
    static final int UNCONFIRMED=0, FAILED=1, NEEDS_VALIDATION=2, PUBLISHED=3;
    private PackageBrokerProtocol() {}
    static boolean operation(int op) { return op>=BEGIN && op<=CANCEL; }
    private static IllegalArgumentException bad() { return new IllegalArgumentException("Invalid package metadata"); }
    private static boolean name(String s) { return s!=null && s.length()<=128 && s.matches("[a-z0-9][a-z0-9+.-]+"); }
    private static boolean version(String s) {
        return s!=null && s.length()<=128 && s.matches("(?:[0-9]+:)?[0-9][a-zA-Z0-9.+~\\-]*") && !s.endsWith("-");
    }
    private static boolean hash(String s) { return s!=null && s.matches("[0-9a-f]{64}"); }

    static final class Intent {
        final int action, scope;
        final String name, version;
        Intent(int action,int scope,String name,String version) {
            if (scope<PERSONAL || scope>SHARED || action<INSTALL || action>REMOVE || name==null || version==null
                    || (action==UPDATE ? !name.isEmpty() || !version.isEmpty()
                        : !name(name) || name.endsWith("+") || name.endsWith("-")
                            || (!version.isEmpty() && !version(version)) || (action==REMOVE && !version.isEmpty()))) throw bad();
            this.action=action;this.scope=scope;this.name=name;this.version=version;
        }
        boolean same(Intent other) {
            return other!=null && action==other.action && scope==other.scope && name.equals(other.name) && version.equals(other.version);
        }
    }

    /** Validate CLI syntax before entering or poisoning the shared native connection. */
    static byte[] payload(int op,long job,Intent intent,String digest) {
        if (!operation(op)) throw bad();
        if (op==BEGIN) {
            if (job!=0 || intent==null || digest!=null) throw bad();
            byte[] n=intent.name.getBytes(StandardCharsets.US_ASCII),v=intent.version.getBytes(StandardCharsets.US_ASCII);
            return ByteBuffer.allocate(16+n.length+v.length).order(ByteOrder.LITTLE_ENDIAN)
                    .putInt(intent.action).putInt(intent.scope).putInt(n.length).putInt(v.length).put(n).put(v).array();
        }
        if (job<=0 || intent!=null || (op==START ? !hash(digest) : digest!=null)) throw bad();
        ByteBuffer b=ByteBuffer.allocate(op==START?72:8).order(ByteOrder.LITTLE_ENDIAN).putLong(job);
        if (op==START) b.put(digest.getBytes(StandardCharsets.US_ASCII));
        return b.array();
    }
    /** Tail must be produced by payload() and retained privately by the connection. */
    static byte[] request(int op,long sequence,long deadline,int user,int serial,long now,byte[] tail) {
        if (!operation(op) || tail==null || (op==BEGIN ? tail.length<16 || tail.length>272
                : tail.length!=(op==START?72:8))) throw bad();
        return RuntimeBrokerProtocol.header(op,sequence,deadline,user,serial,now,32+tail.length).put(tail).array();
    }

    static final class Change {
        final String name,architecture,before,after;
        final int reason;
        Change(Map<String,Object> v) {
            fields(v,"name","architecture","before","after","reason");
            name=text(v,"name");architecture=text(v,"architecture");before=text(v,"before");after=text(v,"after");
            reason=(int)number(v,"reason",1,2);
            if (!name(name) || !(architecture.equals("all") || architecture.equals("arm64"))
                    || (!before.isEmpty() && !version(before)) || (!after.isEmpty() && !version(after)) || (before.isEmpty() && after.isEmpty())) throw bad();
        }
    }
    static final class PrivateRemoval {
        static final int REMOVED=1, COMMON=2, DEPENDENCY=3;
        final String architecture,before,after;
        final int result;
        PrivateRemoval(Map<String,Object> v) {
            fields(v,"architecture","before","after","result");
            architecture=text(v,"architecture");before=text(v,"before");after=text(v,"after");
            result=(int)number(v,"result",REMOVED,DEPENDENCY);
            if (!(architecture.equals("all") || architecture.equals("arm64")) || !version(before)
                    || (!after.isEmpty() && !version(after)) || (result==REMOVED)!=after.isEmpty()) throw bad();
        }
    }
    static final class Metadata {
        final Intent intent;
        final String digest;
        final long validUntil;
        final int phase,outcome,waitStatus,error;
        final String image,sharedBase;
        final long imageBytes;
        final List<Change> changes;
        final PrivateRemoval privateRemoval;
        Metadata(Map<String,Object> v,boolean review) {
            intent=new Intent((int)number(v,"action",1,3),(int)number(v,"scope",1,2),text(v,"package"),text(v,"version"));
            boolean removal=review && intent.scope==PERSONAL && intent.action==REMOVE;
            fields(v,review ? (removal
                    ? new String[]{"action","scope","package","version","digest","validUntil","changes","privateRemoval"}
                    : new String[]{"action","scope","package","version","digest","validUntil","changes"})
                    : new String[]{"action","scope","package","version","digest","validUntil","phase","outcome","waitStatus","error","generation"});
            privateRemoval=removal?new PrivateRemoval(object(v.get("privateRemoval"))):null;
            digest=text(v,"digest");validUntil=number(v,"validUntil",0,Long.MAX_VALUE);
            if (!digest.isEmpty() && !hash(digest)) throw bad();
            if (review) {
                if (!hash(digest)) throw bad();
                List<?> raw=list(v.get("changes"));
                if ((!removal && raw.isEmpty()) || raw.size()>64) throw bad();
                List<Change> checked=new ArrayList<>();String previous="";boolean requested=intent.action==UPDATE;
                for (Object item:raw) {
                    Change c=new Change(object(item));
                    // Dependency resolution may install and remove in the same plan.
                    // Every archive-bearing effect still requires repository expiry.
                    boolean selection=c.before.equals(c.after);
                    if (selection && (raw.size()!=1 || intent.scope!=PERSONAL || intent.action!=INSTALL
                            || !c.name.equals(intent.name) || c.reason!=1)) throw bad();
                    if (previous.compareTo(c.name)>=0 || (!c.after.isEmpty() && !selection && validUntil==0)) throw bad();
                    if (c.name.equals(intent.name)) {
                        if ((!removal && c.after.isEmpty()!=(intent.action==REMOVE))
                                || (!intent.version.isEmpty() && !intent.version.equals(c.after))) throw bad();
                        if (removal && (!c.architecture.equals(privateRemoval.architecture)
                                || !c.before.equals(privateRemoval.before) || !c.after.equals(privateRemoval.after)
                                || (!c.after.isEmpty() && c.reason!=(privateRemoval.result==PrivateRemoval.DEPENDENCY?2:1)))) throw bad();
                        requested=true;
                    }
                    previous=c.name;checked.add(c);
                }
                if (removal && !requested) {
                    if (!privateRemoval.before.equals(privateRemoval.after)) throw bad();
                    requested=true;
                }
                if (!requested || (intent.action!=REMOVE && validUntil==0
                        && !checked.get(0).before.equals(checked.get(0).after))) throw bad();
                changes=Collections.unmodifiableList(checked);
                phase=REVIEWED;outcome=UNCONFIRMED;waitStatus=error=0;image=sharedBase="";imageBytes=0;
            } else {
                phase=(int)number(v,"phase",SELECTING,SEALED);outcome=(int)number(v,"outcome",UNCONFIRMED,PUBLISHED);
                waitStatus=(int)number(v,"waitStatus",0,65535);error=(int)number(v,"error",0,4095);
                if (phase>=REVIEWED && phase<=PUBLISHING && !hash(digest)) throw bad();
                Map<String,Object> generation=object(v.get("generation"));fields(generation,"image","sharedBase","bytes");
                image=text(generation,"image");sharedBase=text(generation,"sharedBase");imageBytes=number(generation,"bytes",0,32L<<30);
                if (outcome==PUBLISHED) {
                    if (phase!=COMPLETE || error!=0 || waitStatus!=0 || !hash(digest) || !hash(image)
                            || imageBytes==0 || imageBytes%4096!=0
                            || (intent.scope==PERSONAL ? !hash(sharedBase) : !sharedBase.isEmpty())) throw bad();
                } else if (!image.isEmpty() || !sharedBase.isEmpty() || imageBytes!=0) throw bad();
                changes=Collections.emptyList();
            }
        }
        void requireIntent(Intent expected) { if (!intent.same(expected)) throw bad(); }
    }
    static final class Reply {
        final int error;
        final long job;
        final Metadata metadata;
        Reply(int error,long job,Metadata metadata) { this.error=error;this.job=job;this.metadata=metadata; }
    }
    static Reply reply(byte[] bytes,int length,int op,long sequence,int user,int serial,long expectedJob,int descriptors) {
        if (!operation(op) || descriptors!=0 || bytes==null || length<PREFIX || length>MAX_REPLY || bytes.length<length
                || (op==BEGIN ? expectedJob!=0 : expectedJob<=0)) throw bad();
        RuntimeBrokerProtocol.Reply header=RuntimeBrokerProtocol.parse(bytes,length,op,sequence,user,serial,length);
        ByteBuffer b=ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        long job=b.getLong(32);int kind=b.getInt(40),size=b.getInt(44);
        if (job<0 || (header.error==0 && job==0) || (op!=BEGIN && job!=expectedJob)
                || size!=length-PREFIX || kind<0 || kind>2 || ((kind==0)!=(size==0))
                || (header.error!=0 && kind!=0) || (header.error==0 && kind!=(op==STATUS?1:op==REVIEW?2:0))) throw bad();
        // Even failed BEGIN may own resources. Keep its job for session cleanup.
        return new Reply(header.error,job,kind==0?null:new Metadata(decode(bytes,length),kind==2));
    }

    private static String text(Map<String,Object> o,String key) {
        Object v=o.get(key);if (!(v instanceof String)) throw bad();return (String)v;
    }
    private static long number(Map<String,Object> o,String key,long low,long high) {
        Object v=o.get(key);if (!(v instanceof Long)) throw bad();long n=(Long)v;if(n<low || n>high)throw bad();return n;
    }
    @SuppressWarnings("unchecked") private static Map<String,Object> object(Object v) {
        if (!(v instanceof Map)) throw bad();return (Map<String,Object>)v;
    }
    private static List<?> list(Object v) { if (!(v instanceof List)) throw bad();return (List<?>)v; }
    private static void fields(Map<String,Object> v,String... names) {
        if (!v.keySet().equals(new HashSet<>(Arrays.asList(names)))) throw bad();
    }
    private static Map<String,Object> decode(byte[] bytes,int length) {
        try {
            String text=StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes,PREFIX,length-PREFIX)).toString();
            try (JsonReader reader=new JsonReader(new StringReader(text))) {
                reader.setLenient(false);
                Map<String,Object> result=object(read(reader,0));
                if (reader.peek()!=JsonToken.END_DOCUMENT) throw bad();
                return result;
            }
        } catch (IOException | IllegalStateException failure) { throw bad(); }
    }
    private static Object read(JsonReader r,int depth) throws IOException {
        if (depth>3) throw bad();
        switch (r.peek()) {
        case BEGIN_OBJECT:
            Map<String,Object> object=new HashMap<>();r.beginObject();
            while (r.hasNext()) {
                String name=r.nextName();
                if (name.length()>32 || object.size()>=16 || object.containsKey(name)) throw bad();
                object.put(name,read(r,depth+1));
            }
            r.endObject();return object;
        case BEGIN_ARRAY:
            List<Object> array=new ArrayList<>();r.beginArray();
            while (r.hasNext()) { if (array.size()>=64) throw bad();array.add(read(r,depth+1)); }
            r.endArray();return array;
        case STRING:
            String value=r.nextString();if (value.length()>128) throw bad();return value;
        case NUMBER:
            String number=r.nextString();if (!number.matches("0|[1-9][0-9]{0,18}")) throw bad();
            try { return Long.parseLong(number); } catch (NumberFormatException failure) { throw bad(); }
        default:throw bad();
        }
    }
}
