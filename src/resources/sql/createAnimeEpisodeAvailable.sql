CREATE TABLE IF NOT EXISTS anime_episode_available(
  media_id INTEGER NOT NULL,
  episode INTEGER NOT NULL,
  path TEXT NOT NULL,
  PRIMARY KEY (media_id, episode),
  FOREIGN KEY (media_id) REFERENCES anime (id)
);
