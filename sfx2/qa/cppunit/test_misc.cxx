/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/types.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <memory>

#include <com/sun/star/beans/Pair.hpp>
#include <com/sun/star/beans/PropertyState.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/document/DocumentProperties.hpp>
#include <com/sun/star/packages/zip/ZipFileAccess.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/frame/XStorable.hpp>
#include <com/sun/star/rdf/Statement.hpp>
#include <com/sun/star/rdf/XDocumentMetadataAccess.hpp>
#include <com/sun/star/rdf/XDocumentRepository.hpp>
#include <com/sun/star/rdf/XRepository.hpp>
#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/ucb/XSimpleFileAccess.hpp>

#include <test/unoapixml_test.hxx>

#include <unotools/ucbstreamhelper.hxx>
#include <comphelper/sequence.hxx>
#include <comphelper/propertysequence.hxx>
#include <comphelper/propertyvalue.hxx>
#include <comphelper/processfactory.hxx>
#include <sfx2/app.hxx>
#include <osl/file.hxx>

using namespace ::com::sun::star;

namespace {

class MiscTest
    : public UnoApiXmlTest
{
public:
    MiscTest()
        : UnoApiXmlTest(u"/sfx2/qa/cppunit/data/"_ustr)
    {
    }

    virtual void registerNamespaces(xmlXPathContextPtr& pXmlXpathCtx) override
    {
        // ODF
        xmlXPathRegisterNs(pXmlXpathCtx, BAD_CAST("office"), BAD_CAST("urn:oasis:names:tc:opendocument:xmlns:office:1.0"));
        xmlXPathRegisterNs(pXmlXpathCtx, BAD_CAST("meta"), BAD_CAST("urn:oasis:names:tc:opendocument:xmlns:meta:1.0"));
        xmlXPathRegisterNs(pXmlXpathCtx, BAD_CAST("dc"), BAD_CAST("http://purl.org/dc/elements/1.1/"));
        // used in testCustomMetadata
        xmlXPathRegisterNs(pXmlXpathCtx, BAD_CAST("foo"), BAD_CAST("http://foo.net"));
        xmlXPathRegisterNs(pXmlXpathCtx, BAD_CAST("baz"), BAD_CAST("http://baz.net"));
    }
};

CPPUNIT_TEST_FIXTURE(MiscTest, testODFCustomMetadata)
{
    uno::Reference<document::XDocumentProperties> const xProps(
        ::com::sun::star::document::DocumentProperties::create(m_xContext));

    OUString const url(m_directories.getURLFromSrc(u"/sfx2/qa/complex/sfx2/testdocuments/CUSTOM.odt"));
    xProps->loadFromMedium(url, uno::Sequence<beans::PropertyValue>());
    CPPUNIT_ASSERT_EQUAL(u""_ustr, xProps->getAuthor());
    uno::Sequence<beans::PropertyValue> mimeArgs({
        beans::PropertyValue(u"MediaType"_ustr, -1, uno::Any(u"application/vnd.oasis.opendocument.text"_ustr), beans::PropertyState_DIRECT_VALUE)
        });
    xProps->storeToMedium(maTempFile.GetURL(), mimeArgs);

    // check that custom metadata is preserved
    xmlDocUniquePtr pXmlDoc = parseExport(u"meta.xml"_ustr);
    assertXPathContent(pXmlDoc, "/office:document-meta/office:meta/bork"_ostr, u"bork"_ustr);
    assertXPath(pXmlDoc, "/office:document-meta/office:meta/foo:bar"_ostr, 1);
    assertXPath(pXmlDoc, "/office:document-meta/office:meta/foo:bar/baz:foo"_ostr, 1);
    assertXPath(pXmlDoc, "/office:document-meta/office:meta/foo:bar/baz:foo[@baz:bar='foo']"_ostr);
    assertXPathContent(pXmlDoc, "/office:document-meta/office:meta/foo:bar/foo:baz"_ostr, u"bar"_ustr);
}

CPPUNIT_TEST_FIXTURE(MiscTest, testNoThumbnail)
{
    // Load a document.
    loadFromFile(u"hello.odt");

    // Save it with the NoThumbnail option and assert that it has no thumbnail.
#ifndef _WIN32
    mode_t nMask = umask(022);
#endif
    uno::Reference<frame::XStorable> xStorable(mxComponent, uno::UNO_QUERY);
    CPPUNIT_ASSERT(xStorable.is());
    uno::Sequence<beans::PropertyValue> aProperties(
        comphelper::InitPropertySequence({ { "NoThumbnail", uno::Any(true) } }));
    osl::File::remove(maTempFile.GetURL());
    xStorable->storeToURL(maTempFile.GetURL(), aProperties);
    uno::Reference<packages::zip::XZipFileAccess2> xZipFile
        = packages::zip::ZipFileAccess::createWithURL(m_xContext, maTempFile.GetURL());
    CPPUNIT_ASSERT(!xZipFile->hasByName(u"Thumbnails/thumbnail.png"_ustr));

#ifndef _WIN32
    // Check permissions of the URL after store.
    osl::DirectoryItem aItem;
    CPPUNIT_ASSERT_EQUAL(osl::DirectoryItem::E_None,
                         osl::DirectoryItem::get(maTempFile.GetURL(), aItem));

    osl::FileStatus aStatus(osl_FileStatus_Mask_Attributes);
    CPPUNIT_ASSERT_EQUAL(osl::DirectoryItem::E_None, aItem.getFileStatus(aStatus));

    // The following checks used to fail in the past, osl_File_Attribute_GrpRead was not set even if
    // umask requested so:
    CPPUNIT_ASSERT(aStatus.getAttributes() & osl_File_Attribute_GrpRead);
    CPPUNIT_ASSERT(aStatus.getAttributes() & osl_File_Attribute_OthRead);

    // Now "save as" again to trigger the "overwrite" case.
    xStorable->storeToURL(maTempFile.GetURL(), {});
    CPPUNIT_ASSERT_EQUAL(osl::DirectoryItem::E_None, aItem.getFileStatus(aStatus));
    // The following check used to fail in the past, result had temp file
    // permissions.
    CPPUNIT_ASSERT(aStatus.getAttributes() & osl_File_Attribute_GrpRead);

    umask(nMask);
#endif
}

CPPUNIT_TEST_FIXTURE(MiscTest, testHardLinks)
{
#ifndef _WIN32
    OUString aTargetDir = m_directories.getURLFromWorkdir(u"/CppunitTest/sfx2_misc.test.user/");
    const OUString aURL(aTargetDir + "hello.odt");
    osl::File::copy(createFileURL(u"hello.odt"), aURL);
    OUString aTargetPath;
    osl::FileBase::getSystemPathFromFileURL(aURL, aTargetPath);
    OString aOld = aTargetPath.toUtf8();
    aTargetPath += ".2";
    OString aNew = aTargetPath.toUtf8();
    int nRet = link(aOld.getStr(), aNew.getStr());
    CPPUNIT_ASSERT_EQUAL(0, nRet);

    mxComponent = loadFromDesktop(aURL, u"com.sun.star.text.TextDocument"_ustr);

    uno::Reference<frame::XStorable> xStorable(mxComponent, uno::UNO_QUERY);
    xStorable->store();

    struct stat buf;
    // coverity[fs_check_call] - this is legitimate in the context of this test
    nRet = stat(aOld.getStr(), &buf);
    CPPUNIT_ASSERT_EQUAL(0, nRet);
    // This failed: hard link count was 1, the hard link broke on store.
    CPPUNIT_ASSERT(buf.st_nlink > 1);

    // Test that symlinks are preserved as well.
    nRet = remove(aNew.getStr());
    CPPUNIT_ASSERT_EQUAL(0, nRet);
    nRet = symlink(aOld.getStr(), aNew.getStr());
    CPPUNIT_ASSERT_EQUAL(0, nRet);
    xStorable->storeToURL(aURL + ".2", {});
    nRet = lstat(aNew.getStr(), &buf);
    CPPUNIT_ASSERT_EQUAL(0, nRet);
    // This failed, the hello.odt.2 symlink was replaced with a real file.
    CPPUNIT_ASSERT(bool(S_ISLNK(buf.st_mode)));
#endif
}

CPPUNIT_TEST_FIXTURE(MiscTest, testOverwrite)
{
    // tdf#60237 - try to overwrite an existing file using the different settings of the Overwrite option
    mxComponent
        = loadFromDesktop(maTempFile.GetURL(), u"com.sun.star.text.TextDocument"_ustr);
    uno::Reference<frame::XStorable> xStorable(mxComponent, uno::UNO_QUERY);
    CPPUNIT_ASSERT(xStorable.is());

    // overwrite the file using the default case of the Overwrite option (true)
    CPPUNIT_ASSERT_NO_THROW(xStorable->storeToURL(maTempFile.GetURL(), {}));

    // explicitly overwrite the file using the Overwrite option
    CPPUNIT_ASSERT_NO_THROW(xStorable->storeToURL(
        maTempFile.GetURL(),
        comphelper::InitPropertySequence({ { "Overwrite", uno::Any(true) } })));

    try
    {
        // overwrite an existing file with the Overwrite flag set to false
        xStorable->storeToURL(maTempFile.GetURL(), comphelper::InitPropertySequence(
                                                      { { "Overwrite", uno::Any(false) } }));
        CPPUNIT_ASSERT_MESSAGE("We expect an exception on overwriting an existing file", false);
    }
    catch (const css::uno::Exception&)
    {
    }
}

std::vector<rdf::Statement>
sortStatementsByPredicate(const uno::Sequence<rdf::Statement>& rStatements)
{
    std::vector<rdf::Statement> aStatements
        = comphelper::sequenceToContainer<std::vector<rdf::Statement>>(rStatements);
    std::sort(aStatements.begin(), aStatements.end(),
              [](const rdf::Statement& a, const rdf::Statement& b) {
                  return a.Predicate->getStringValue() < b.Predicate->getStringValue();
              });
    return aStatements;
}

CPPUNIT_TEST_FIXTURE(MiscTest, testRDFa)
{
    auto verify = [this](bool bIsExport) {
        uno::Reference<text::XTextDocument> xTextDocument(mxComponent, uno::UNO_QUERY);
        uno::Reference<container::XEnumerationAccess> xParaEnumAccess(xTextDocument->getText(),
                                                                      uno::UNO_QUERY);
        uno::Reference<container::XEnumeration> xParaEnum = xParaEnumAccess->createEnumeration();

        uno::Reference<rdf::XDocumentMetadataAccess> xDocumentMetadataAccess(mxComponent,
                                                                             uno::UNO_QUERY);
        uno::Reference<rdf::XRepository> xRepo = xDocumentMetadataAccess->getRDFRepository();
        uno::Reference<rdf::XDocumentRepository> xDocRepo(xRepo, uno::UNO_QUERY);
        CPPUNIT_ASSERT(xDocRepo);

        {
            // RDFa: 1
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("1"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(!xResult.Second);
        }
        {
            // RDFa: 2
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("2"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(!xResult.Second);
        }

        OUString aSubject3;
        OUString aSubject4;
        OUString aSubject5;
        {
            // RDFa: 3
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            aSubject3 = aStatements[0].Subject->getStringValue();
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("3"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(!xResult.Second);
        }
        {
            // RDFa: 4
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            aSubject4 = aStatements[0].Subject->getStringValue();
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("4"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(!xResult.Second);
        }
        {
            // RDFa: 5
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            aSubject5 = aStatements[0].Subject->getStringValue();
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("5"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(!xResult.Second);
        }

        CPPUNIT_ASSERT(aSubject3 != aSubject4);
        CPPUNIT_ASSERT_EQUAL(aSubject3, aSubject5);

        {
            // RDFa: 6
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);

            std::vector<rdf::Statement> aStatements = sortStatementsByPredicate(xResult.First);
            CPPUNIT_ASSERT_EQUAL(size_t(2), aStatements.size());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("6"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[1].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:baz"), aStatements[1].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("6"), aStatements[1].Object->getStringValue());
            CPPUNIT_ASSERT(!xResult.Second);
        }
        {
            // RDFa: 7
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            std::vector<rdf::Statement> aStatements = sortStatementsByPredicate(xResult.First);
            CPPUNIT_ASSERT_EQUAL(size_t(3), aStatements.size());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("7"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[1].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:baz"), aStatements[1].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("7"), aStatements[1].Object->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[2].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[2].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("7"), aStatements[2].Object->getStringValue());
            CPPUNIT_ASSERT(!xResult.Second);
        }
        {
            // RDFa: 8
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("a fooish bar"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(xResult.Second);
        }
        {
            // RDFa: 9
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("a fooish bar"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(xResult.Second);
        }
        {
            // RDFa: 10
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("a fooish bar^^uri:bar"),
                                 aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(xResult.Second);
        }
        {
            // RDFa: 11
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("11^^uri:bar"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(!xResult.Second);
        }
        {
            // RDFa: 12
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            OUString aSubject;
            if (bIsExport)
                aSubject = maTempFile.GetURL() + "/content.xml";
            else
                aSubject = createFileURL(u"TESTRDFA.odt") + "/content.xml";
            CPPUNIT_ASSERT_EQUAL(aSubject, aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("12"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(!xResult.Second);
        }
        {
            // RDFa: 13
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("a fooish bar"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(xResult.Second);
        }
        {
            // RDFa: 14
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aStatements.getLength());

            CPPUNIT_ASSERT_EQUAL(OUString("uri:foo"), aStatements[0].Subject->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("uri:bar"), aStatements[0].Predicate->getStringValue());
            CPPUNIT_ASSERT_EQUAL(OUString("a fooish bar"), aStatements[0].Object->getStringValue());
            CPPUNIT_ASSERT(xResult.Second);
        }

        // Remaining rdfs should be empty
        do
        {
            uno::Reference<rdf::XMetadatable> xPara(xParaEnum->nextElement(), uno::UNO_QUERY);
            ::beans::Pair<uno::Sequence<rdf::Statement>, sal_Bool> xResult
                = xDocRepo->getStatementRDFa(xPara);
            uno::Sequence<rdf::Statement> aStatements = xResult.First;
            CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aStatements.getLength());
        } while (xParaEnum->hasMoreElements());
    };

    loadFromFile(u"TESTRDFA.odt");
    verify(/*bIsExport*/ false);
    saveAndReload(u"writer8"_ustr);
    verify(/*bIsExport*/ true);
}

CPPUNIT_TEST_FIXTURE(MiscTest, testTdf123293)
{
    const uno::Reference<uno::XComponentContext> xContext(comphelper::getProcessComponentContext(),
                                                     css::uno::UNO_SET_THROW);
    const uno::Reference<com::sun::star::ucb::XSimpleFileAccess> xFileAccess(
        xContext->getServiceManager()->createInstanceWithContext(
            u"com.sun.star.ucb.SimpleFileAccess"_ustr, xContext),
        uno::UNO_QUERY_THROW);
    const uno::Reference<io::XInputStream> xInputStream(xFileAccess->openFileRead(createFileURL(u"TESTRDFA.odt") ),
                                                   uno::UNO_SET_THROW);
    uno::Sequence<beans::PropertyValue> aLoadArgs{ comphelper::makePropertyValue(
        "InputStream", xInputStream) };
    mxComponent = mxDesktop->loadComponentFromURL(u"private:stream"_ustr, u"_blank"_ustr, 0, aLoadArgs);
    CPPUNIT_ASSERT(mxComponent.is());
    uno::Reference<rdf::XDocumentMetadataAccess> xDocumentMetadataAccess(mxComponent, uno::UNO_QUERY);
    uno::Reference<rdf::XRepository> xRepo = xDocumentMetadataAccess->getRDFRepository();
    uno::Reference<rdf::XDocumentRepository> xDocRepo(xRepo, uno::UNO_QUERY);
    CPPUNIT_ASSERT(xDocRepo);
}
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
