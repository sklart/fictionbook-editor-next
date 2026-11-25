<?xml version="1.0" encoding="us-ascii"?>

<X:stylesheet version="1.0"
  xmlns:X="http://www.w3.org/1999/XSL/Transform"
  xmlns:F="http://www.gribuser.ru/xml/fictionbook/2.0"
  xmlns:L="http://www.w3.org/1999/xlink">

  <X:output method="html" 
    encoding="utf-8"
    version="5"
    indent="yes"/>

  <!-- create <img> elements or not? -->
  <X:param name="saveimages" select="0"/>
  <!-- Emit FB2 binary images as data: URIs rather than external files. -->
  <X:param name="embedimages" select="0"/>
  <X:param name="imgprefix" select="''"/>
  <X:param name="includedesc" select="1"/>
  <X:param name="tocdepth" select="3"/>
  <X:param name="customcss" select="''"/>
  <X:param name="imagemaxwidth" select="0"/>
  <X:param name="imagemaxheight" select="0"/>  <X:param name="includetoc" select="1"/>
  <X:param name="includemetadata" select="1"/>
  <X:param name="includeannotation" select="1"/>
  <X:param name="includetitleinfo" select="1"/>
  <X:param name="includedocumentinfo" select="1"/>
  <X:param name="includepublishinfo" select="1"/>
  <X:param name="includehistory" select="1"/>
  <X:param name="includeauthors" select="1"/>
  <X:param name="includetranslators" select="1"/>
  <X:param name="includecustominfo" select="1"/>
  <X:param name="style" select="0"/>
  <X:param name="fontfamily" select="0"/>
  <X:param name="customfontfamily" select="''"/>
  <X:param name="fontsize" select="0"/>
  <X:param name="lineheight" select="0"/>
  <X:param name="contentmaxwidth" select="0"/>
  <X:param name="pagemargins" select="1"/>
  <X:param name="textalignment" select="1"/>
  <X:param name="headingalignment" select="0"/>
  <X:param name="covermode" select="0"/>
  <X:param name="noteplacement" select="0"/>

  <X:key name="binary-by-id" match="F:binary" use="@id"/>
  <X:key name="note-ref" match="F:a[@type='note' or @F:type='note']" use="@L:href"/>
  
  <!-- note about attributes: this template can be called
       with both qualified and unqualified attributes,
       so we have to accept both -->

  <X:template match="/">
    <html>
	<X:if test="/F:FictionBook/F:description/F:title-info/F:lang">
	  <X:attribute name="lang"><X:value-of select="/F:FictionBook/F:description/F:title-info/F:lang"/></X:attribute>
	</X:if>
      <head>
	<meta charset="utf-8"/>
	<!-- stick in a simple html stylesheet -->
        <style type="text/css">
	  body { font-family: <X:choose><X:when test="$fontfamily = 1">sans-serif</X:when><X:when test="$fontfamily = 2">system-ui, sans-serif</X:when><X:when test="$fontfamily = 3 and string-length(normalize-space($customfontfamily))">"<X:value-of select="$customfontfamily"/>"</X:when><X:otherwise>serif</X:otherwise></X:choose>;<X:if test="$fontsize &gt; 0"> font-size: <X:value-of select="$fontsize"/>px;</X:if><X:if test="$lineheight &gt; 0"> line-height: <X:value-of select="$lineheight div 100"/>;</X:if><X:if test="$contentmaxwidth &gt; 0"> max-width: <X:value-of select="$contentmaxwidth"/>px; margin-left: auto; margin-right: auto;</X:if><X:choose><X:when test="$pagemargins = 0"> margin: 0.5em;</X:when><X:when test="$pagemargins = 2"> margin: 3em;</X:when></X:choose> }
	  h1,h2,h3,h4,h5,h6 { text-align: <X:choose><X:when test="$headingalignment = 1">left</X:when><X:otherwise>center</X:otherwise></X:choose>; font-weight: bold; }
	  h1 { font-size: 200%; }
	  h2 { font-size: 180%; }
	  h3 { font-size: 160%; }
	  h4 { font-size: 140%; }
	  h5 { font-size: 120%; }
	  h6 { font-size: 100%; }
	  p { margin-top: 0pt; margin-bottom: 0.1em; text-indent: 3em; text-align: <X:choose><X:when test="$textalignment = 0">left</X:when><X:otherwise>justify</X:otherwise></X:choose>; }
	  img { border: none; max-width: <X:choose><X:when test="$imagemaxwidth &gt; 0"><X:value-of select="$imagemaxwidth"/>px</X:when><X:otherwise>100%</X:otherwise></X:choose>; <X:if test="$imagemaxheight &gt; 0">max-height: <X:value-of select="$imagemaxheight"/>px; </X:if>height: auto; }
	  .cover { display: block; margin: 0 auto;<X:if test="$covermode = 1"> max-width: 42em;</X:if><X:if test="$covermode = 2"> max-width: 100vw; max-height: 100vh;</X:if> }
	  blockquote { margin-left: 1em; margin-right: 1em; color: rgb(228,175,0); }
          li { display: block; }
	  .epigraph { max-width: 25em; float: right; margin: 0pt; font-size: smaller; }
	  .epigraph p { text-indent: 2em; }
	  .espace { clear: right; margin: 0pt; padding: 0pt; }
	  .author { margin-left: 3em; margin-bottom: 1em; color: rgb(192,64,64); }
	  .poem { margin: 2em; }
	  .stanza + .stanza { margin-top: 1em; }
	  .stanza p { text-align: left; text-indent: 0pt; }
	  .note { position: relative; top: -0.3em; font-size: smaller; text-decoration: none; }
	  .note-back { margin-left: 0.4em; font-size: smaller; text-decoration: none; }
	  .annotation { border: solid black 1px; font-size: smaller;
			margin-left: 2em; margin-right: 2em; padding-left: 0.3em; }
	  .toclink { text-decoration: none; }
	  .props { margin-top: 2em; border: none; border-collapse: collapse; }
	  .props td { border: solid black 1px; vertical-align: top; padding-left: 0.3em; }
	  .fb2-table { border-collapse: collapse; margin: 1em auto; max-width: 100%; }
	  .fb2-table td, .fb2-table th { border: solid black 1px; padding: 0.3em; vertical-align: top; }
	  .propsec { text-align: center; font-weight: bolder; border: none !important }
	  td p { text-indent: 0pt; }
	  .center { text-align: center; }
	  <X:if test="$style = 1">body { background: #f7f1e3; color: #2f2419; } .props td { border-color: #8b7355; }</X:if>
	  <X:if test="$style = 2">body { max-width: 72em; } .props, .props td { border: none; } blockquote { color: inherit; }</X:if>
	  <X:value-of select="$customcss" disable-output-escaping="yes"/>
	</style>
	<title>
		<X:variable name="authors" select="/F:FictionBook/F:description/F:title-info/F:author"/>
		<X:variable name="book-title" select="normalize-space(/F:FictionBook/F:description/F:title-info/F:book-title)"/>
		<X:choose>
		  <X:when test="$authors and $book-title"><X:for-each select="$authors"><X:if test="position() &gt; 1"><X:text>, </X:text></X:if><X:call-template name="author-title"/></X:for-each><X:text> &#x2014; </X:text><X:value-of select="$book-title"/></X:when>
		  <X:when test="$book-title"><X:value-of select="$book-title"/></X:when>
		  <X:when test="$authors"><X:for-each select="$authors"><X:if test="position() &gt; 1"><X:text>, </X:text></X:if><X:call-template name="author-title"/></X:for-each></X:when>
		  <X:otherwise>FictionBook</X:otherwise>
		</X:choose>
	</title>
	<!-- optional FB2 metadata -->
	<X:if test="$includemetadata and $includetitleinfo and $includeauthors"><X:for-each select="/F:FictionBook/F:description/F:title-info/F:author">
	  <meta name="author"><X:attribute name="content"><X:call-template name="author-prop"/></X:attribute></meta>
	</X:for-each></X:if>
	<X:if test="$includemetadata and $includetitleinfo"><meta name="title"><X:attribute name="content"><X:value-of select="/F:FictionBook/F:description/F:title-info/F:book-title"/></X:attribute></meta></X:if>
	<X:if test="$includemetadata and $includetitleinfo and $includeannotation and /F:FictionBook/F:description/F:title-info/F:annotation">
	  <meta name="description"><X:attribute name="content"><X:value-of select="normalize-space(/F:FictionBook/F:description/F:title-info/F:annotation)"/></X:attribute></meta>
	</X:if>      </head>
      <body>
        <X:if test="$saveimages">
        <X:choose>
          <X:when test="string-length(/F:FictionBook/F:description/F:title-info/F:coverpage/F:image/@L:href)">
            <div class="center">
	      <img class="cover">
                <X:choose>
                  <X:when test="starts-with(/F:FictionBook/F:description/F:title-info/F:coverpage/F:image/@L:href,'#')">                
                        <X:attribute name="src">
                          <X:choose>
                            <X:when test="$embedimages"><X:value-of select="concat('data:', key('binary-by-id', substring(/F:FictionBook/F:description/F:title-info/F:coverpage/F:image/@L:href,2))/@content-type, ';base64,', translate(key('binary-by-id', substring(/F:FictionBook/F:description/F:title-info/F:coverpage/F:image/@L:href,2)), ' &#x9;&#xA;&#xD;', ''))"/></X:when>
                            <X:otherwise><X:value-of select="concat($imgprefix,substring(/F:FictionBook/F:description/F:title-info/F:coverpage/F:image/@L:href,2))"/></X:otherwise>
                          </X:choose>
                        </X:attribute>
                  </X:when>
                  <X:otherwise>
                    <X:attribute name="src">
                      <X:value-of select="concat($imgprefix,/F:FictionBook/F:description/F:title-info/F:coverpage/F:image/@L:href)"/>
                    </X:attribute>
                  </X:otherwise>
                </X:choose>
              </img>
            </div>
          </X:when>
        </X:choose>
        </X:if>
	<!-- image from the first body -->
	<X:apply-templates select="/F:FictionBook/F:body[1]/F:image[1]" mode="first"/>
	<!-- title from the first body -->
	<X:apply-templates select="/F:FictionBook/F:body[1]/F:title[1]" mode="first"/>
	<!-- table of contents -->
	<X:if test="$includetoc and //F:section/F:title">
	  <h4>Table of contents</h4>
          <ul>
	    <X:apply-templates mode="toc" select="/F:FictionBook/F:body/F:section"/>
	    <!-- Metadata links follow the v2 master and child flags. -->
	    <X:if test="$includemetadata and $includeannotation and /F:FictionBook/F:description/F:title-info/F:annotation">
	      <li><p><a class="toclink" href="#_fbh_annotation">Annotation</a></p></li>
	    </X:if>
	    <X:if test="$includemetadata and ($includetitleinfo or $includedocumentinfo or $includepublishinfo or $includecustominfo)">
	      <li><p><a class="toclink" href="#_fbh_description">Document information</a></p></li>
	    </X:if>
	  </ul>
	</X:if>
	<!-- main text -->
	<X:apply-templates select="/F:FictionBook/F:body"/>
	<X:if test="$noteplacement = 1 and //F:section[(@id or @F:id) and key('note-ref', concat('#', @id | @F:id))]">
	  <section class="notes"><h4>Notes</h4><X:apply-templates select="//F:section[(@id or @F:id) and key('note-ref', concat('#', @id | @F:id))]" mode="note"/></section>
	</X:if>	<!-- generate description -->
	<X:if test="$includemetadata and ($includeannotation or $includetitleinfo or $includedocumentinfo or $includepublishinfo or $includecustominfo)">
	  <hr/>
	  <!-- annotation -->
	  <X:if test="$includeannotation"><X:apply-templates select="/F:FictionBook/F:description/F:title-info/F:annotation"/></X:if>
	  <!-- description properties -->
	  <a id="_fbh_description" name="_fbh_description"/>
	  <table class="props">
	    <X:if test="$includetitleinfo"><X:apply-templates select="/F:FictionBook/F:description/F:title-info"/></X:if>
	    <X:if test="$includedocumentinfo"><X:apply-templates select="/F:FictionBook/F:description/F:document-info"/></X:if>
	    <X:if test="$includepublishinfo"><X:apply-templates select="/F:FictionBook/F:description/F:publish-info"/></X:if>
	    <X:if test="$includecustominfo and /F:FictionBook/F:description/F:custom-info">
	      <tr><td class="propsec" colspan="2">Custom Info</td>
		<X:apply-templates select="/F:FictionBook/F:description/F:custom-info"/>
	      </tr>
	    </X:if>
	  </table>
	</X:if>
      </body>
    </html>
  </X:template>

  <!-- text body -->
  <X:template match="F:body">
    <!-- provide a visual separator between body elements -->
    <!-- <hr/> -->
    <X:apply-templates/>
  </X:template>

  <!-- text sections; note bodies are suppressed from their source location only
       when a placement mode explicitly re-emits them below. -->
  <X:template match="F:section">
    <X:choose>
      <X:when test="$noteplacement != 0 and (@id or @F:id) and key('note-ref', concat('#', @id | @F:id))"/>
      <X:otherwise><X:call-template name="render-section"/></X:otherwise>
    </X:choose>
  </X:template>

  <X:template name="note-backlinks">
    <X:if test="(@id or @F:id) and key('note-ref', concat('#', @id | @F:id))">
      <X:for-each select="key('note-ref', concat('#', @id | @F:id))">
        <a class="note-back">
          <X:attribute name="href"><X:text>#_note_ref_</X:text><X:value-of select="generate-id()"/></X:attribute>
          <X:text>&#x21A9;</X:text><X:if test="last() &gt; 1"><X:value-of select="position()"/></X:if>
        </a><X:if test="position() != last()"><X:text> </X:text></X:if>
      </X:for-each>
    </X:if>
  </X:template>

  <X:template name="render-section">
    <section>
      <X:call-template name="id"/>
      <X:if test="F:title"><a><X:attribute name="id"><X:text>_toc_</X:text><X:value-of select="generate-id()"/></X:attribute><X:attribute name="name"><X:text>_toc_</X:text><X:value-of select="generate-id()"/></X:attribute></a></X:if>
      <X:apply-templates/>
      <X:call-template name="note-backlinks"/>
      <X:if test="$noteplacement = 2 and not(ancestor::F:section) and descendant::F:section[(@id or @F:id) and key('note-ref', concat('#', @id | @F:id))]">
        <section class="notes"><h4>Notes</h4><X:apply-templates select="descendant::F:section[(@id or @F:id) and key('note-ref', concat('#', @id | @F:id))]" mode="note"/></section>
      </X:if>
    </section>
  </X:template>

  <X:template match="F:section" mode="note">
    <section class="note-entry"><X:call-template name="id"/><X:apply-templates/><X:call-template name="note-backlinks"/></section>
  </X:template>

  <!-- special case to insert table of contents after the first header -->
  <X:template match="/F:FictionBook/F:body[1]/F:title[1]">
    <!-- swallow this header in normal mode -->
  </X:template>

  <X:template match="/F:FictionBook/F:body[1]/F:title[1]" mode="first">
    <X:call-template name="sb-title"/>
  </X:template>

  <X:template match="/F:FictionBook/F:body[1]/F:image[1]">
    <!-- swallow this header in normal mode -->
  </X:template>

  <X:template match="/F:FictionBook/F:body[1]/F:image[1]" mode="first">
    <X:call-template name="sb-image"/>
  </X:template>

  <X:template match="F:title">
    <X:call-template name="sb-title"/>
  </X:template>

  <!-- section and body titles -->
  <X:template name="sb-title">
    <!-- figure an appropriate heading -->
    <X:variable name="level" select="2+count(ancestor::F:section)+count(ancestor::F:description)"/>
    <X:choose>
      <!-- always use h4 when nesting level is >4 -->
      <X:when test="$level>4">
	<h4><X:call-template name="hx-title"/></h4>
      </X:when>
      <X:otherwise>
	<!-- compute an appropriate heading element -->
	<X:element name="h{$level}"><X:call-template name="hx-title"/></X:element>
      </X:otherwise>
    </X:choose>
  </X:template>

  <!-- we have to jump through the hoops because Hx elements can't have block
       level content in them -->
  <X:template name="hx-title">
    <X:for-each select="F:p | F:empty-line">
      <X:if test="position()>1">
	<br/>
      </X:if>
      <X:apply-templates/>
    </X:for-each>
  </X:template>

  <!-- annotations -->
  <X:template match="F:annotation">
    <div class="annotation">
	  <a id="_fbh_annotation" name="_fbh_annotation"/>
      <X:apply-templates/>
    </div>
  </X:template>

  <!-- plain paragraphs -->
  <X:template match="F:p">
    <X:call-template name="id"/>
    <p><X:apply-templates/></p>
  </X:template>

  <!-- empty-lines -->
  <X:template match="F:empty-line">
    <p>&#160;</p>
  </X:template>

  <!-- text decoration inside paragraphs: strong, emphasis -->
  <X:template match="F:strong">
    <strong><X:apply-templates/></strong>
  </X:template>
  <X:template match="F:emphasis">
    <em><X:apply-templates/></em>
  </X:template>

  <!-- hyperlinks in text -->
  <X:template match="F:a">
    <a>
      <X:attribute name="href"><X:value-of select="@L:href"/></X:attribute>
      <X:if test='@type="note" or @F:type="note"'>
	<X:attribute name="class">note</X:attribute>
	<X:attribute name="id"><X:text>_note_ref_</X:text><X:value-of select="generate-id()"/></X:attribute>
      </X:if>
      <X:apply-templates/>
    </a>
  </X:template>

  <!-- images -->
  <X:template match="F:image">
    <X:call-template name="sb-image"/>
  </X:template>

  <X:template name="sb-image">
    <X:if test="$saveimages">
      <div class="center"><img>
	<X:choose>
	  <X:when test="$embedimages and starts-with(@L:href,'#')">
	    <X:attribute name="src">
	      <X:value-of select="concat('data:', key('binary-by-id', substring(@L:href,2))/@content-type, ';base64,', translate(key('binary-by-id', substring(@L:href,2)), ' &#x9;&#xA;&#xD;', ''))"/>
	    </X:attribute>
	  </X:when>
	  <X:when test="starts-with(@L:href,'#')">
	    <X:attribute name="src">
	      <X:value-of select="concat($imgprefix,substring(@L:href,2))"/>
	    </X:attribute>
	  </X:when>
	  <X:otherwise>
	    <X:attribute name="src">
	      <X:value-of select="concat($imgprefix,@L:href)"/>
	    </X:attribute>
	  </X:otherwise>
	</X:choose>
      </img></div>
    </X:if>
  </X:template>

  <!-- hyperlink targets -->
  <X:template name="id">
    <X:if test="@id | @F:id">
	  <a><X:attribute name="id"><X:value-of select="@id | @F:id"/></X:attribute><X:attribute name="name"><X:value-of select="@id | @F:id"/></X:attribute></a>
    </X:if>
  </X:template>

  <!-- style elements are converted into span -->
  <X:template match="F:style">
    <span>
      <X:attribute name="class">
	<X:text>xc_</X:text><X:value-of select="@name | @F:name"/>
      </X:attribute>
      <X:apply-templates/>
    </span>
  </X:template>

  <!-- subtitles -->
  <X:template match="F:subtitle">
    <h5><X:apply-templates/></h5>
  </X:template>

  <!-- epigraphs -->
  <X:template match="F:epigraph">
    <X:call-template name="id"/>
    <!-- move epigraph to the far right -->
    <div class="epigraph">
      <X:apply-templates/>
    </div>
    <!-- force other text to start below the epigraph -->
    <div class="espace"/>
  </X:template>

  <!-- citations -->
  <X:template match="F:cite">
    <X:call-template name="id"/>
    <blockquote>
      <X:apply-templates/>
    </blockquote>
  </X:template>

  <!-- poems -->
  <X:template match="F:poem">
    <X:call-template name="id"/>
    <!-- add some space around it -->
    <div class="poem">
      <X:apply-templates/>
    </div>
  </X:template>

  <X:template match="F:stanza">
    <div class="stanza">
      <X:apply-templates/>
    </div>
  </X:template>

  <!-- FB2 tables are preserved instead of being swallowed by the fallback rule. -->
  <X:template match="F:table">
    <X:call-template name="id"/>
    <table class="fb2-table"><X:apply-templates/></table>
  </X:template>

  <X:template match="F:tr">
    <tr><X:apply-templates/></tr>
  </X:template>

  <X:template match="F:td | F:th">
    <X:element name="{local-name()}">
      <X:if test="@colspan"><X:attribute name="colspan"><X:value-of select="@colspan"/></X:attribute></X:if>
      <X:if test="@rowspan"><X:attribute name="rowspan"><X:value-of select="@rowspan"/></X:attribute></X:if>
      <X:apply-templates/>
    </X:element>
  </X:template>

  <X:template match="F:v">
    <p><X:apply-templates/></p>
  </X:template>

  <!-- epigraph/citation/poem author -->
  <X:template match="F:text-author">
    <p class="author"><X:apply-templates/></p>
  </X:template>

  <!-- everything else is just swallowed and ignored -->
  <X:template match="F:*"/>

  <!-- properties -->
  <X:template match="F:title-info">
    <tr><td class="propsec" colspan="2">Title Info</td></tr>
    <tr>
      <td>Genre<X:if test="count(F:genre) &gt; 1">s</X:if></td>
      <td>
	<X:for-each select="F:genre">
	  <X:text> </X:text>
	  <X:value-of select="."/>
	  <X:if test="@match &lt; 100 or @F:match &lt; 100">
	    <X:text>[</X:text><X:value-of select="@match | @F:match"/><X:text>%]</X:text>
	  </X:if>
	</X:for-each>
      </td>
    </tr>
    <X:if test="$includeauthors">
      <tr>
        <td>Author<X:if test="count(F:author) &gt; 1">s</X:if></td>
        <td>
	  <X:for-each select="F:author">
	    <X:if test="position() &gt; 1">,</X:if>
	    <X:call-template name="author-prop"/>
	  </X:for-each>
        </td>
      </tr>
    </X:if>
    <tr><td>Title</td><td><X:value-of select="F:book-title"/></td></tr>
    <X:if test="F:keywords">
      <tr><td>Keywords</td><td><X:value-of select="F:keywords"/></td></tr>
    </X:if>
    <X:apply-templates select="F:date"/>
    <X:if test="F:lang">
      <tr><td>Language</td><td><X:value-of select="F:lang"/></td></tr>
    </X:if>
    <X:if test="F:src-lang">
      <tr><td>Source Language</td><td><X:value-of select="F:src-lang"/></td></tr>
    </X:if>
    <X:if test="$includetranslators and F:translator">
      <tr>
	<td>Translator<X:if test="count(F:translator) &gt; 1">s</X:if></td>
	<td>
	  <X:for-each select="F:translator">
	    <X:if test="position() &gt; 1">,</X:if>
	    <X:call-template name="author-prop"/>
	  </X:for-each>
	</td>
      </tr>
    </X:if>
  </X:template>

  <X:template match="F:document-info">
    <tr><td class="propsec" colspan="2">Document Info</td></tr>
    <tr>
      <td>Author<X:if test="count(F:translator) &gt; 1">s</X:if></td>
      <td>
	<X:for-each select="F:author">
	  <X:if test="position() &gt; 1">,</X:if>
	  <X:call-template name="author-prop"/>
	</X:for-each>
      </td>
    </tr>
    <X:if test="F:program-used">
      <tr><td>Program used</td><td><X:value-of select="F:program-used"/></td></tr>
    </X:if>
    <X:apply-templates select="F:date"/>
    <X:if test="F:src-url">
      <tr><td>Source URL</td>
	<td>
	  <a>
	    <X:attribute name="href"><X:value-of select="F:src-url"/></X:attribute>
	    <X:value-of select="F:src-url"/>
	  </a>
	</td>
      </tr>
    </X:if>
    <X:if test="F:src-ocr">
      <tr><td>Source OCR</td><td><X:value-of select="F:src-ocr"/></td></tr>
    </X:if>
    <tr><td>ID</td><td><X:value-of select="F:id"/></td></tr>
    <tr><td>Version</td><td><X:value-of select="F:version"/></td></tr>
    <X:if test="$includehistory and F:history">
      <tr><td>History</td><td><X:apply-templates select="F:history/*"/></td></tr>
    </X:if>
  </X:template>

  <X:template match="F:publish-info">
    <tr><td class="propsec" colspan="2">Publisher Info</td></tr>
    <X:if test="F:book-name">
      <tr><td>Book name</td><td><X:value-of select="F:book-name"/></td></tr>
    </X:if>
    <X:if test="F:publisher">
      <tr><td>Publisher</td><td><X:value-of select="F:publisher"/></td></tr>
    </X:if>
    <X:if test="F:city">
      <tr><td>City</td><td><X:value-of select="F:city"/></td></tr>
    </X:if>
    <X:if test="F:year">
      <tr><td>Year</td><td><X:value-of select="F:year"/></td></tr>
    </X:if>
    <X:if test="F:isbn">
      <tr><td>ISBN</td><td><X:value-of select="F:isbn"/></td></tr>
    </X:if>
  </X:template>

  <X:template match="F:custom-info">
    <tr><td><X:value-of select="@info-type | @F:info-type"/></td><td><X:value-of select="."/></td></tr>
  </X:template>

  <X:template match="F:date">
    <tr>
      <td>Date</td>
      <td>
	<X:value-of select="."/>
	<X:if test="string(@value | @F:value) != concat(string(),'-01-01') and string() != string(@value | @F:value)">
	  <X:text> (</X:text><X:value-of select="@value | @F:value"/><X:text>)</X:text>
	</X:if>
      </td>
    </tr>
  </X:template>

  <X:template name="author-prop">
    <X:value-of select="F:first-name"/><X:text> </X:text>
    <X:value-of select="F:middle-name"/><X:text> </X:text>
    <X:value-of select="F:last-name"/>
    <X:if test="F:nickname">
      <X:text> [</X:text><X:value-of select="F:nickname"/>]
    </X:if>
    <X:if test="F:email">
      <X:text> &lt;</X:text><X:value-of select="F:email"/>&gt;
    </X:if>
  </X:template>

  <X:template name="author-title">
    <X:for-each select="F:first-name | F:middle-name | F:last-name | F:nickname">
      <X:if test="position() &gt; 1"><X:text> </X:text></X:if><X:value-of select="normalize-space(.)"/>
    </X:for-each>
  </X:template>

  <!-- table of contents generator -->
  <X:template match="F:section" mode="toc">
    <!-- only include sections that have a title -->
    <X:if test="F:title | .//F:section/F:title">
      <li>
	<X:if test="F:title">
	  <p><a class="toclink">
	      <X:attribute name="href">
		<X:text>#_toc_</X:text><X:value-of select="generate-id()"/>
	      </X:attribute>
	      <!-- insert text content -->
	      <X:for-each select="F:title/F:p">
		<X:if test="position()>1"><X:text> </X:text></X:if>
		<X:value-of select="."/>
	      </X:for-each>
	  </a></p>
	</X:if>
	<X:if test=".//F:section/F:title and count(ancestor::F:section) &lt; $tocdepth">
	  <ul><X:apply-templates select="F:section" mode="toc"/></ul>
	</X:if>
      </li>
    </X:if>
  </X:template>

</X:stylesheet>
