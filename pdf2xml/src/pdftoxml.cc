//==========================================================================
//
// pdftoxml.cc (based on pdftotext.cc, Copyright 1997-2003 Glyph & Cog, LLC)
//
// author: Sophie Andrieu
// 04-2006
//
// Xerox Research Centre Europe
//
//===========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "parseargs.h"
#include "GString.h"
#include "gmem.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "XmlOutputDev.h"
#include "CharTypes.h"
#include "UnicodeMap.h"
#include "Error.h"
#include "config.h"
#include "Parameters.h"
#include "Outline.h"

#include "PDFDocXrce.h"

#include <sys/types.h>
#include <dirent.h>

#include <iostream>
using namespace std;

#include "ConstantsUtils.h"
using namespace ConstantsUtils;

#include "ConstantsXML.h"
using namespace ConstantsXML;

void removeDataAlreadyExist(GString *dir);

static int firstPage = 1;
static int lastPage = 0;
static GBool physLayout = gTrue;
static GBool verbose = gFalse;

static char cfgFileName[256] = "";

static GBool noText = gFalse;
static GBool noImage = gFalse;
static GBool outline = gFalse;
static GBool cutPages = gFalse;
static GBool blocks = gFalse;
static GBool fullFontName = gFalse;
static GBool noImageInline = gFalse;
static GBool annots = gFalse;

static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static GBool quiet = gFalse;
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;
static char namespaceUri[256] = "\001";

static ArgDesc argDesc[] = {
  {"-f",       argInt,      &firstPage,     0,
   "first page to convert"},
  {"-l",       argInt,      &lastPage,      0,
   "last page to convert"},
  {"-verbose",     argFlag,     &verbose,      0,
   "display pdf attributes"},
  {"-noText", argFlag,     &noText,  0,
   "do not extract textual objects"},
  {"-noImage", argFlag,     &noImage,  0,
   "do not extract Images (Bitmap and Vectorial)"},
  {"-noImageInline", argFlag,     &noImageInline,  0,
   "do not include images inline in the stream"},
  {"-outline", argFlag,     &outline,  0,
   "create an outline file xml"},
  {"-annots", argFlag,     &annots,  0,
   "create an annotations file xml"},
  {"-cutPages", argFlag,     &cutPages,  0,
   "cut all pages in separately files"},
  {"-blocks", argFlag,     &blocks,  0,
   "add blocks informations whithin the structure"},
  {"-fullFontName", argFlag,     &fullFontName,  0,
   "fonts names are not normalized"},
  {"-nsURI", argString,     namespaceUri,  sizeof(namespaceUri),
   "add the specified namespace URI"},   
  {"-opw",     argString,   ownerPassword,  sizeof(ownerPassword),
   "owner password (for encrypted files)"},
  {"-upw",     argString,   userPassword,   sizeof(userPassword),
   "user password (for encrypted files)"},
  {"-q",       argFlag,     &quiet,         0,
   "don't print any messages or errors"},
  {"-v",       argFlag,     &printVersion,  0,
   "print copyright and version info"},
  {"-h",       argFlag,     &printHelp,     0,
   "print usage information"},
  {"-help",    argFlag,     &printHelp,     0,
   "print usage information"},
  {"--help",   argFlag,     &printHelp,     0,
   "print usage information"},
  {"-?",       argFlag,     &printHelp,     0,
   "print usage information"},
  {NULL}
};

 /** 
 * Main method which execute pdftoxml tool <br/>
 * This file pdftoxml.cc is based on pdftotext.cc, Copyright 1997-2003 Glyph & Cog, LLC <br/>
 * Usage: pdftoxml [options] <PDF-file> [<XML-file>] <br/>
 * -f <int> : first page to convert<br/>
 * -l <int> : last page to convert<br/>
 * -verbose : display pdf attributes<br/>
 * -noText : do not extract textual objects<br/>
 * -noImage : do not extract images (Bitmap and Vectorial)<br/>
 * -noImageInline : do not include images inline in the stream<br/>
 * -outline : create an outline file xml<br/>
 * -annots : create an annotaitons file xml<br/>
 * -cutPages : cut all pages in separately files<br/>
 * -blocks : add blocks informations whithin the structure<br/>
 * -fullFontName : fonts names are not normalized<br/>
 * -nsURI : add the specified namespace URI<br/>
 * -q : don't print any messages or errors<br/>
 * -v : print copyright and version information<br/>
 * -h : print usage information<br/>
 * -help : print usage information<br/>
 * --help : print usage information<br/>
 * -? : print usage information<br/>
 * <br/>
 * Xerox Research Centre Europe <br/>
 * @date 04-2006
 * @param argc The arguments number
 * @param argv[] The arguments list that the user configured
 * @author Sophie Andrieu
 * @version xpdf 3.01
 */
int main(int argc, char *argv[]) {
  PDFDocXrce *doc;
  
  GString *fileName;
  GString *textFileName;
  GString *dataDirName;
  GString *shortFileName;
  GString *annotationfile;
  GString *ownerPW, *userPW;
  GString *nsURI;
  GString *cmd;
  XmlOutputDev *xmlOut;
  GBool ok;
  char *p;
  int exitCode;
  char *temp;
  
  exitCode = 99;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc < 2 || argc > 3 || printVersion || printHelp) { 
    fprintf(stderr, PDFTOXML_NAME);
    fprintf(stderr, " version ");
    fprintf(stderr, PDFTOXML_VERSION);
    fprintf(stderr, "\n");
    fprintf(stderr, "(Based on Xpdf version %s, %s)\n", xpdfVersion, xpdfCopyright);
    fprintf(stderr, "%s\n", "Copyright 2004-2006 XEROX XRCE");
    if (!printVersion) {
      printUsage("pdftoxml", "<PDF-file> [<xml-file>]", argDesc);
    }
    goto err0;
  }
  fileName = new GString(argv[1]);
  cmd = new GString();
  
  globalParams = new GlobalParams(cfgFileName);
  
  // Parameters specifics to pdftoxml
  parameters = new Parameters();
  
  if(noImage){
    parameters->setDisplayImage(gFalse);
    cmd->append("-noImage ");
  }
  else{
    parameters->setDisplayImage(gTrue);
  }

  if(noText){
    parameters->setDisplayText(gFalse);
    cmd->append("-noText ");
  }
  else{
    parameters->setDisplayText(gTrue);
  }

  if(outline){
    parameters->setDisplayOutline(gFalse);
    cmd->append("-outline ");
  }
  else{
    parameters->setDisplayOutline(gTrue);
  }
  
  if(cutPages){
    parameters->setCutAllPages(gFalse);
    cmd->append("-cutPages ");
  }
  else{
    parameters->setCutAllPages(gTrue);
  }
  
  if(blocks){
    parameters->setDisplayBlocks(gTrue);
    cmd->append("-blocks ");
  }
  else{
    parameters->setDisplayBlocks(gFalse);
  }
  
  if(fullFontName){
    parameters->setFullFontName(gTrue);
    cmd->append("-fullFontName ");
  }
  else{
    parameters->setFullFontName(gFalse);
  }

  if(noImageInline){
    parameters->setImageInline(gTrue);
    cmd->append("-noImageInline ");
  }
  else{
    parameters->setImageInline(gFalse);
  }
  
  if (quiet) {
    globalParams->setErrQuiet(quiet);
  }

  if (verbose){
  	cmd->append("-verbose ");
  }
  
  // open PDF file
  if (ownerPassword[0] != '\001') {
    ownerPW = new GString(ownerPassword);
  } else {
    ownerPW = NULL;
  }
  if (userPassword[0] != '\001') {
    userPW = new GString(userPassword);
  } else {
    userPW = NULL;
  }
  
  if (namespaceUri[0] != '\001') {
    nsURI = new GString(namespaceUri);
    cmd->append("-nsURI ")->append(nsURI)->append(" ");
  } else {
    nsURI = NULL;
  }
  
  // Create the object PDF doc
  doc = new PDFDocXrce(fileName, ownerPW, userPW); 
  
  if (userPW) {
    delete userPW;
  }
  if (ownerPW) {
    delete ownerPW;
  }
  if (!doc->isOk()) {
    exitCode = 1;
    goto err2;
  }

 if (!doc->okToCopy())
      fprintf(stderr, "\n\nYou are not supposed to copy this document...\n\n");

  // IF output XML file name was given in command line
  if (argc == 3) {
    textFileName = new GString(argv[2]);
    temp = textFileName->getCString() + textFileName->getLength() - 4;
     if (!strcmp(temp, EXTENSION_XML) || !strcmp(temp, EXTENSION_XML_MAJ)) {
      	shortFileName = new GString(textFileName->getCString(), textFileName->getLength() - 4);
    }else {
      	shortFileName = new GString(textFileName);
    }
  }
  // ELSE we build the output XML file name with the PDF file name 
  else {
    p = fileName->getCString() + fileName->getLength() - 4;
    if (!strcmp(p, EXTENSION_PDF) || !strcmp(p, EXTENSION_PDF_MAJ)) {
      	textFileName = new GString(fileName->getCString(), fileName->getLength() - 4);
		shortFileName = new GString(textFileName);
	
    } else {
      	textFileName = fileName->copy();
      	shortFileName = new GString(textFileName);
    }
    textFileName->append(EXTENSION_XML);
  }

  // For the annotations XML file
  if (annots){  
  	annotationfile = new GString(shortFileName);
  	annotationfile->append("_");
  	annotationfile->append(NAME_ANNOT);
  	annotationfile->append(EXTENSION_XML);
  	cmd->append("-annots ");
  }
  
  // Get page range
  if (firstPage < 1) {
    firstPage = 1;
  }
  if (firstPage!=1){
  	char* temp=(char*)malloc(10*sizeof(char));
	sprintf(temp,"%d",firstPage);
  	cmd->append("-f ")->append(temp)->append(" ");
  	free(temp);
  }
  
  if (lastPage!=0){
  	int last = lastPage;
  	if (lastPage > doc->getNumPages()){
  		last = doc->getNumPages();
  	}
  	char* temp=(char*)malloc(10*sizeof(char));
	sprintf(temp,"%d",last);
  	cmd->append("-l ")->append(temp)->append(" ");
  	free(temp);
  }
  if (lastPage < 1 || lastPage > doc->getNumPages()) {
    lastPage = doc->getNumPages();
  }

  // Write xml file
  xmlOut = new XmlOutputDev(textFileName, fileName, physLayout, verbose, nsURI, cmd);

  if (xmlOut->isOk()) {
  	
  	if (outline){
  		if (doc->getOutline()){
  			xmlOut->initOutline(doc->getNumPages());  			 
        	xmlOut->generateOutline(doc->getOutline()->getItems(), doc, 0);	
        	xmlOut->closeOutline(shortFileName);
       		fprintf(stderr, "Outline XML file creation\n");
       		fflush(stderr);
  		}else{
				fprintf(stderr, "This document doesn't contain outline.\n");
  				fflush(stderr);
		}
  	}
  	// We clean the data directory if it is already exist 
  	dataDirName = new GString(textFileName);
  	dataDirName->append(NAME_DATA_DIR);
  	removeDataAlreadyExist(dataDirName);
  	
  	// Xml file to store annotations informations
  	if (annots){  	
  		xmlDocPtr  docAnnotXml;
  		xmlNodePtr docroot;
  		docAnnotXml = xmlNewDoc((const xmlChar*)VERSION);
  		docAnnotXml->encoding = xmlStrdup((const xmlChar*)ENCODING_UTF8);
  		docroot = xmlNewNode(NULL,(const xmlChar*)TAG_ANNOTATIONS);
  		xmlDocSetRootElement(docAnnotXml,docroot);
  	  
    	doc->displayPages(xmlOut, docroot, firstPage, lastPage, 72, 72, 0, gTrue, gTrue, gFalse);

    	xmlSaveFile(annotationfile->getCString(),docAnnotXml);
  		xmlFreeDoc(docAnnotXml);
  	}else{
  	  	doc->displayPages(xmlOut, NULL, firstPage, lastPage, 72, 72, 0, gTrue, gTrue, gFalse);
  	}
  } 
  else {
    delete xmlOut;
    exitCode = 2;
    goto err3;
  }
  delete xmlOut;
  exitCode = 0;
  // clean up
  
  if (nsURI) {
    delete nsURI;
  }
  
 err3:
  delete textFileName;
  
 err2:
  delete doc;
  delete globalParams;
  delete parameters;
  delete cmd;
  
 err0:
  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);
  return exitCode;
}

/** Remove all files which are in data directory of file pdf if it is already exist 
 * @param dir The directory name where we remove all data */
void removeDataAlreadyExist(GString *dir) {
	GString *file;
	struct dirent *lecture;
	DIR *rep;
	rep = opendir(dir->getCString());
	if (rep != NULL) {
		while ((lecture = readdir(rep))){
			file = new GString(dir);
			file->append("/");
			file->append(lecture->d_name);
			std::remove(file->getCString());
   		}
   		if (file) delete file;
   		closedir(rep);
	}
}
