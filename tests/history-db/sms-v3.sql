BEGIN TRANSACTION;
PRAGMA user_version = 3;
PRAGMA foreign_keys = ON;
CREATE TABLE mime_type (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE
);
CREATE TABLE files (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name TEXT,
  url TEXT NOT NULL UNIQUE,
  path TEXT,
  mime_type_id INTEGER REFERENCES mime_type(id),
  status INT,
  size INTEGER
);
CREATE TABLE audio (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  file_id INTEGER NOT NULL UNIQUE,
  duration INTEGER,
  FOREIGN KEY(file_id) REFERENCES files(id)
);
CREATE TABLE image (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  file_id INTEGER NOT NULL UNIQUE,
  width INTEGER,
  height INTEGER,
  FOREIGN KEY(file_id) REFERENCES files(id)
);
CREATE TABLE video (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  file_id INTEGER NOT NULL UNIQUE,
  width INTEGER,
  height INTEGER,
  duration INTEGER,
  FOREIGN KEY(file_id) REFERENCES files(id)
);
CREATE TABLE users (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL,
  alias TEXT,
  avatar_id INTEGER REFERENCES files(id),
  type INTEGER NOT NULL,
  UNIQUE (username, type)
);
INSERT INTO users VALUES(1,'SMS',NULL,NULL,1);
INSERT INTO users VALUES(2,'MMS',NULL,NULL,1);
CREATE TABLE accounts (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  user_id INTEGER NOT NULL REFERENCES users(id),
  password TEXT,
  enabled INTEGER DEFAULT 0,
  protocol INTEGER NOT NULL,
  UNIQUE (user_id, protocol)
);
INSERT INTO accounts VALUES(1,1,NULL,0,1);
INSERT INTO accounts VALUES(2,2,NULL,0,2);
CREATE TABLE threads (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  alias TEXT,
  avatar_id INTEGER REFERENCES files(id),
  account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
  type INTEGER NOT NULL,
  encrypted INTEGER DEFAULT 0,
  UNIQUE (name, account_id, type)
);
CREATE TABLE thread_members (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  thread_id INTEGER NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
  user_id INTEGER NOT NULL REFERENCES users(id),
  UNIQUE (thread_id, user_id)
);
CREATE TABLE messages (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  uid TEXT NOT NULL,
  thread_id INTEGER NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
  sender_id INTEGER REFERENCES users(id),
  user_alias TEXT,
  body TEXT NOT NULL,
  body_type INTEGER NOT NULL,
  direction INTEGER NOT NULL,
  time INTEGER NOT NULL,
  status INTEGER,
  encrypted INTEGER DEFAULT 0,
  preview_id INTEGER REFERENCES files(id),
  UNIQUE (uid, thread_id, body, time)
);
ALTER TABLE threads ADD COLUMN last_read_id INTEGER REFERENCES messages(id);
ALTER TABLE threads ADD COLUMN visibility INT NOT NULL DEFAULT 0;

INSERT INTO users VALUES(3,'+12133210011',NULL,NULL,1);
INSERT INTO users VALUES(4,'Mobile@5G',NULL,NULL,1);
INSERT INTO users VALUES(5,'5555',NULL,NULL,1);
INSERT INTO users VALUES(6,'+919876121212',NULL,NULL,1);
INSERT INTO users VALUES(7,'+12133456789',NULL,NULL,1);

INSERT INTO threads VALUES(1,'+12133210011','+12133210011',NULL,1,0,0,NULL,0);
INSERT INTO threads VALUES(2,'Mobile@5G','Mobile@5G',NULL,1,0,0,NULL,0);
INSERT INTO threads VALUES(3,'5555','5555',NULL,1,0,0,NULL,0);
INSERT INTO threads VALUES(4,'+919876121212','+919876121212',NULL,1,0,0,NULL,0);
INSERT INTO threads VALUES(5,'+12133456789','(213) 345-6789',NULL,1,0,0,NULL,0);

INSERT INTO thread_members VALUES(1,1,3);
INSERT INTO thread_members VALUES(2,2,4);
INSERT INTO thread_members VALUES(3,3,5);
INSERT INTO thread_members VALUES(4,4,6);
INSERT INTO thread_members VALUES(5,5,7);

INSERT INTO messages VALUES(NULL,'1a1cbd44-7526-4032-9665-45aee085ab65',1,3,NULL,'I''m fine',1,1,1600074789,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'259478cf-64b3-44e1-9b1c-5d1773edc601',1,3,NULL,'Hi',1,1,1600074685,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'22be2899-8c1e-4501-ab33-979c356a6764',1,3,NULL,'Hello',1,-1,1600074686,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'af65adc0-2d80-4de8-83bb-9bf9ea4ebd5d',1,3,NULL,'How are you?',1,-1,1600074687,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'271fe95c-5d47-4ffe-ae62-7f2f6b749711',2,4,NULL,'Get Unlimmtted 5G',1,1,1600074809,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'601f2a66-e6a6-4083-9dce-e5d78fb57520',2,4,NULL,'Get Unlimitted 5G',1,1,1600074800,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'4dafafd9-734c-4f86-b1ec-09aa327b8a88',3,5,NULL,'Free unlimitted internet 4 99$',1,1,1600074802,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'1218070f-c820-40e1-bd33-5099d894683a',4,6,NULL,'Hello',1,1,1600075652,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'9abcc777-5b06-4570-9b83-48603a49add2',4,6,NULL,'Hi.',1,-1,1600075658,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'c5b99952-5517-4620-8f28-fb97f5017cee',5,7,NULL,'May I call you?',1,-1,1600075789,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'f098e603-5ac1-4d5a-bcad-c7fe84c91252',5,7,NULL,'Sure, you may call me',1,1,1600075889,NULL,0,NULL);

COMMIT;
