PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('mmap_status','-1');
INSERT INTO "meta" VALUES('version','69');
INSERT INTO "meta" VALUES('last_compatible_version','69');
INSERT INTO "meta" VALUES('Builtin Keyword Version','96');
CREATE TABLE token_service (service VARCHAR PRIMARY KEY NOT NULL,encrypted_token BLOB);
CREATE TABLE keywords (id INTEGER PRIMARY KEY,short_name VARCHAR NOT NULL,keyword VARCHAR NOT NULL,favicon_url VARCHAR NOT NULL,url VARCHAR NOT NULL,safe_for_autoreplace INTEGER,originating_url VARCHAR,date_created INTEGER DEFAULT 0,usage_count INTEGER DEFAULT 0,input_encodings VARCHAR,suggest_url VARCHAR,prepopulate_id INTEGER DEFAULT 0,created_by_policy INTEGER DEFAULT 0,instant_url VARCHAR,last_modified INTEGER DEFAULT 0,sync_guid VARCHAR,alternate_urls VARCHAR,search_terms_replacement_key VARCHAR,image_url VARCHAR,search_url_post_params VARCHAR,suggest_url_post_params VARCHAR,instant_url_post_params VARCHAR,image_url_post_params VARCHAR,new_tab_url VARCHAR, last_visited INTEGER DEFAULT 0);
INSERT INTO "keywords" VALUES(2,'Google','google.com','http://www.google.com/favicon.ico','{google:baseURL}search?q={searchTerms}&{google:RLZ}{google:originalQueryForSuggestion}{google:assistedQueryStats}{google:searchFieldtrialParameter}{google:iOSSearchLanguage}{google:searchClient}{google:sourceId}{google:instantExtendedEnabledParameter}{google:contextualSearchVersion}ie={inputEncoding}',1,'',0,0,'UTF-8','{google:baseSuggestURL}search?{google:searchFieldtrialParameter}client={google:suggestClient}&gs_ri={google:suggestRid}&xssi=t&q={searchTerms}&{google:inputType}{google:cursorPosition}{google:currentPageUrl}{google:pageClassification}{google:searchVersion}{google:sessionToken}{google:prefetchQuery}sugkey={google:suggestAPIKeyParameter}',1,0,'{google:baseURL}webhp?sourceid=chrome-instant&{google:RLZ}{google:forceInstantResults}{google:instantExtendedEnabledParameter}ie={inputEncoding}',0,'3967f1bd-7913-440d-aab5-319ae178837a','["{google:baseURL}#q={searchTerms}","{google:baseURL}search#q={searchTerms}","{google:baseURL}webhp#q={searchTerms}","{google:baseURL}s#q={searchTerms}","{google:baseURL}s?q={searchTerms}"]','espv','{google:baseURL}searchbyimage/upload','','','','encoded_image={google:imageThumbnail},image_url={google:imageURL},sbisrc={google:imageSearchSource},original_width={google:imageOriginalWidth},original_height={google:imageOriginalHeight}','{google:baseURL}_/chrome/newtab?{google:RLZ}{google:instantExtendedEnabledParameter}ie={inputEncoding}',0);
INSERT INTO "keywords" VALUES(3,'Bing','bing.com','https://www.bing.com/s/a/bing_p.ico','https://www.bing.com/search?q={searchTerms}&PC=U316&FORM=CHROMN',1,'',0,0,'UTF-8','https://www.bing.com/osjson.aspx?query={searchTerms}&language={language}&PC=U316',3,0,'',0,'edf5b23b-f8c6-4235-80a2-c686b9de3e37','[]','','https://www.bing.com/images/detail/search?iss=sbi&FORM=CHROMI#enterInsights','','','','imgurl={google:imageURL}','https://www.bing.com/chrome/newtab',0);
INSERT INTO "keywords" VALUES(4,'Yahoo!','yahoo.com','https://search.yahoo.com/favicon.ico','https://search.yahoo.com/search?ei={inputEncoding}&fr=crmas&p={searchTerms}',1,'',0,0,'UTF-8','https://search.yahoo.com/sugg/chrome?output=fxjson&appid=crmas&command={searchTerms}',2,0,'',0,'56b59942-da06-4f53-841b-a9ef28f50ac8','[]','','','','','','','',0);
INSERT INTO "keywords" VALUES(5,'AOL','aol.com','http://search.aol.com/favicon.ico','http://search.aol.com/aol/search?q={searchTerms}',1,'',0,0,'UTF-8','http://autocomplete.search.aol.com/autocomplete/get?output=json&it=&q={searchTerms}',35,0,'',0,'1a485f3a-b545-4265-8515-d49914865ebd','[]','','','','','','','',0);
INSERT INTO "keywords" VALUES(6,'Ask','ask.com','http://sp.ask.com/sh/i/a16/favicon/favicon.ico','http://www.ask.com/web?q={searchTerms}',1,'',0,0,'UTF-8','http://ss.ask.com/query?q={searchTerms}&li=ff',4,0,'',0,'6725c539-3c39-4518-b58a-2a0623007920','[]','','','','','','','',0);
CREATE TABLE autofill (name VARCHAR, value VARCHAR, value_lower VARCHAR, date_created INTEGER DEFAULT 0, date_last_used INTEGER DEFAULT 0, count INTEGER DEFAULT 1, PRIMARY KEY (name, value));
CREATE TABLE credit_cards ( guid VARCHAR PRIMARY KEY, name_on_card VARCHAR, expiration_month INTEGER, expiration_year INTEGER, card_number_encrypted BLOB, date_modified INTEGER NOT NULL DEFAULT 0, origin VARCHAR DEFAULT '', use_count INTEGER NOT NULL DEFAULT 0, use_date INTEGER NOT NULL DEFAULT 0, billing_address_id VARCHAR);
CREATE TABLE autofill_profiles ( guid VARCHAR PRIMARY KEY, company_name VARCHAR, street_address VARCHAR, dependent_locality VARCHAR, city VARCHAR, state VARCHAR, zipcode VARCHAR, sorting_code VARCHAR, country_code VARCHAR, date_modified INTEGER NOT NULL DEFAULT 0, origin VARCHAR DEFAULT '', language_code VARCHAR, use_count INTEGER NOT NULL DEFAULT 0, use_date INTEGER NOT NULL DEFAULT 0);
CREATE TABLE autofill_profile_names ( guid VARCHAR, first_name VARCHAR, middle_name VARCHAR, last_name VARCHAR, full_name VARCHAR);
CREATE TABLE autofill_profile_emails ( guid VARCHAR, email VARCHAR);
CREATE TABLE autofill_profile_phones ( guid VARCHAR, number VARCHAR);
CREATE TABLE autofill_profiles_trash ( guid VARCHAR);
CREATE TABLE masked_credit_cards (id VARCHAR,status VARCHAR,name_on_card VARCHAR,type VARCHAR,last_four VARCHAR,exp_month INTEGER DEFAULT 0,exp_year INTEGER DEFAULT 0, billing_address_id VARCHAR);
CREATE TABLE unmasked_credit_cards (id VARCHAR,card_number_encrypted VARCHAR, use_count INTEGER NOT NULL DEFAULT 0, use_date INTEGER NOT NULL DEFAULT 0, unmask_date INTEGER NOT NULL DEFAULT 0);
CREATE TABLE server_card_metadata (id VARCHAR NOT NULL,use_count INTEGER NOT NULL DEFAULT 0, use_date INTEGER NOT NULL DEFAULT 0);
CREATE TABLE server_addresses (id VARCHAR,company_name VARCHAR,street_address VARCHAR,address_1 VARCHAR,address_2 VARCHAR,address_3 VARCHAR,address_4 VARCHAR,postal_code VARCHAR,sorting_code VARCHAR,country_code VARCHAR,language_code VARCHAR, recipient_name VARCHAR, phone_number VARCHAR);
CREATE TABLE server_address_metadata (id VARCHAR NOT NULL,use_count INTEGER NOT NULL DEFAULT 0, use_date INTEGER NOT NULL DEFAULT 0);
CREATE INDEX autofill_name ON autofill (name);
CREATE INDEX autofill_name_value_lower ON autofill (name, value_lower);
COMMIT;
