#!/usr/bin/python
import z_markdown as md
import os
import re
import functools

def filterDir( fun , path = "./files" ):
    return [f for f in os.listdir( path )
            if fun( os.path.join( path, f ))]


def getFilesInDir( path="./files" ):
    return filterDir( os.path.isfile , path )

def getDirsInDir( path = "./files" ):
    return filterDir( os.path.isdir , path )

def filterByExtension( l, ext ):
    rr = re.compile( R".*\."+ext+"$")
    return filter( lambda x : rr.match( x ) , l )

def compose(*functions):
    return functools.reduce(lambda f, g: lambda x: f(g(x)), functions)

def passert( a, b ):
    if a != b:
        print a,"!=",b
    assert( a == b )

def a(x): return x+1
def b(x): return x*x

passert( compose( b,a )(1) , 4 )
del a
del b


assert( filterByExtension( ["bob.py","bil.pyc"], "py" ) == ["bob.py"] )
assert( filterByExtension( ["bob.py","bilpy"], "py" ) == ["bob.py"] )
assert( filterByExtension( ["bob.py","bil.pyc"], "" ) == [] )

def removeExtensions( l ):
    rr = re.compile( R"\.[^\.]*$" )
    return map( lambda x : rr.sub( '', x ) , l )

assert( removeExtensions(['bob.py']) == ['bob'] )
assert( removeExtensions(['bob.bob.py']) == ['bob.bob'] )
assert( removeExtensions(['bob..py']) == ['bob.'] )

def addExtensions( l , e ):
    return map ( lambda x : addExtension( x,e) , l )

def dropWhile( f , l ):
    while len(l) > 0 and f(l[0]):
        l = l[1:]
    return l

passert( dropWhile( lambda x : x < 3 , [1,2,3,4] ) , [3,4] )
passert( dropWhile( lambda x : x < 3 , [] ) , [] )

def addExtension( a, e ):
    return a +"."+e

'''get the titles of the links to the pdfs
this is the first line in the markdown'''
def getName( fname ):
    autoStub = "Autogenerated Stub"

    l = "Name Not Set" #not replaced unless name actually found in below try block
    try:
        with file( fname, "rb" ) as f:
            lines = f.readlines()
            '''remove leading #s'''
            lines = map ( lambda y : dropWhile( lambda x : x == '#' , y.strip() ) , lines )
            lines = filter ( lambda x : len( x ) > 0 , lines )
            if len( lines ) > 0 :
                l = lines[0]

    except IOError as e:
        with file( fname , "wb" ) as f:
            f.write(autoStub +"\n")
        print "fill out: ",fname

    if l.strip() == autoStub :
        print "Need to fill out: ",fname

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
        if len( a ) <= 1 :
            l = ""
        else:
            l = a[1:]
            l = concat( l )

    s += l
    return md.markdownByString( s )


def combine( mds , pdfs ):
    z = zip( mds, pdfs )
    return map( lambda x : combine_single( x[0], x[1] ) , z )

def intersperse( l , extra ):
    ret = []
    for i in l:
        ret.append( i )
        ret.append( extra )

    ret = ret[:-1]
    return ret

def do_dir( path ):
    pdfs = filterByExtension( getFilesInDir( path ), "pdf")
    pdfs = map ( lambda x : os.path.join( path , x ) , pdfs )
    mds = addExtensions( removeExtensions( pdfs ) , "md" )

    return concat( intersperse( combine( mds , pdfs ), "<hr>" ) )


def main():
    out =  do_dir( "./files" )

    l = []
    with file( "index.html", "rb") as f:
        l = f.readlines()

    with file( "index_old.html","wb") as o:
        o.writelines( l )

    with file( "index.html", "wb") as f:
        f.write( out )

main()
