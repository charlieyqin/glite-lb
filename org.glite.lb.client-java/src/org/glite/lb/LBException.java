package org.glite.lb;

public class LBException extends Exception {
	
    public LBException(Throwable e) {
		super(e);
	}

    public LBException(String s) { 
		super(s);
	}
    
}