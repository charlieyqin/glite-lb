import org.glite.lb.*;
import org.glite.jobid.Jobid;

public class CreamTest {

public static void main(String[] args)
{

   int i;
   String srv = null,socket = null,prefix = null,lib = "glite_lb_sendviasocket";

   for (i = 0; i < args.length; i++) {
	if (args[i].equals("-m")) srv = args[++i];
	else if (args[i].equals("-s")) socket = args[++i];
	else if (args[i].equals("-f")) prefix = args[++i];
	else if (args[i].equals("-l")) lib = args[++i];		/* needs java.library.path */
   }

   try {
	String[] srvpart = srv.split(":");
	int srvport = Integer.parseInt(srvpart[1]);
	Jobid	job = new Jobid(srvpart[0],srvport);

	LBCredentials	cred = new LBCredentials(System.getenv("X509_USER_PROXY"),"/etc/grid-security/certificates");


	ContextDirect	ctxd = new ContextDirect(srvpart[0],srvport);
	ctxd.setCredentials(cred);
	ctxd.setSource(Sources.EDG_WLL_SOURCE_CREAM_CORE);
	ctxd.setJobid(job);
	ctxd.setSeqCode(new SeqCode(SeqCode.CREAM,"no_seqcodes_with_cream"));


/* initial registration goes directly */
	EventRegJob	reg = new EventRegJob();
	reg.setNs("https://where.is.cream:1234");
	reg.setJobtype(EventRegJob.Jobtype.JOBTYPE_CREAM);
	ctxd.log(reg);

	System.out.println("JOBID="+job);

	ContextIL	ctx = new ContextIL(prefix,socket,lib);
	ctx.setSource(Sources.EDG_WLL_SOURCE_CREAM_CORE);
	ctx.setJobid(job);
	ctx.setSeqCode(new SeqCode(SeqCode.CREAM,"no_seqcodes_with_cream"));
	ctx.setUser(ctxd.getUser());

/* 2nd registration with JDL, via IL */
	reg.setJdl("[\n\ttest = \"hellow, world\";\n]");
	ctx.log(reg);

	Event e = new EventCREAMStart();
	ctx.log(e);
	

   } catch (Exception e)
   {
	System.err.println("Oops");
	e.printStackTrace();
   }

}



}
