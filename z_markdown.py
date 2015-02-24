import sh
import os

def markdownByFileName( fname ):
    return str( sh.markdown(fname) )

def markdownByString( s ):
    return str( sh.markdown( _in=s ) )
