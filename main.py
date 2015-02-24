#!/usr/bin/python
import z_markdown as md
import os
import re

def getFilesInDir( path="./files" ):
    ret = [
            f for f in os.listdir( path )
            if os.path.isfile( os.path.join( path,f ))
            ]
    return ret

def filterByExtension( l, ext ):
    rr = re.compile( R".*\."+ext+"$")
    return filter( lambda x : rr.match( x ) , l )

def removeExtensions( l ):
    rr = re.compile( R"\.[^\.]*$" )
    return map( lambda x : rr.sub( '', x ) , l )

assert( removeExtensions(['bob.py']) == ['bob'] )
assert( removeExtensions(['bob.bob.py']) == ['bob.bob'] )
assert( removeExtensions(['bob..py']) == ['bob.'] )

assert( filterByExtension( ["bob.py","bil.pyc"], "py" ) == ["bob.py"] )
assert( filterByExtension( ["bob.py","bilpy"], "py" ) == ["bob.py"] )
assert( filterByExtension( ["bob.py","bil.pyc"], "" ) == [] )

def addExtensions( l , e ):
    return map ( lambda x : addExtension( x,e) , l )

def addExtension( a, e ):
    return a +"."+e

'''get the titles of the links to the pdfs
this is the first line in the markdown'''
def getName( fname ):
    l=""
    done=False
    with file( fname, "rb" ) as f:
        for l in f:
            if l != "":
                while len( l ) > 0 and l[0] == '#' :
                    l = l[1:]
                done=True

            if done == True :
                break
    if l == "":
        return "Name Not Set"
    else :
        return l.strip()

def getNames( l ):
    return map ( getName , l )

def concat( l ):
    return reduce( lambda x,y : x + y , l )

def combine_single( _md , pdf ):
    name = getName( _md )
    s = ""
    s += "<br>"*2
    s += "["+ name +"]("+pdf+")  \n"

    l=[]
    with file( _md ,"rb" ) as f :
        a = f.readlines()
        if len( a ) == 1 :
            l = ""
        else:
            l = a[1:]
            l = concat( l )

    s += l
    return md.markdownByString( s )


def combine( mds , pdfs ):
    z = zip( mds, pdfs )
    return map( lambda x : combine_single( x[0], x[1] ) , z )

def main():
    pdfs = filterByExtension( getFilesInDir( "./files" ), "pdf")
    pdfs = map ( lambda x : "./files/"+x , pdfs )
    mds = addExtensions( removeExtensions( pdfs ) , "md" )

    out =  concat( combine ( mds , pdfs ) )

    l = []
    with file( "index.html", "rb") as f:
        l = f.readlines()

    with file( "index_old.html","wb") as o:
        o.writelines( l )

    with file( "index.html", "wb") as f:
        f.write( out )


main()