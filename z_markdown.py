import sh
import os

def markdownByFileName( fname ):
    return str( sh.markdown_py(fname) )

def markdownByString( s ):
    return str( sh.markdown_py( _in=s ) )
